<?php
// ============================================================
//  Polenta Quotes – API
//
//  Routen:
//    GET  /api/api.php              → alle approved Quotes als JSON-Array
//    GET  /api/api.php?winner=true  → { "winner": "..." }  (für ESP32)
//    POST /api/api.php  { "action": "submit", "text": "..." }  → Quote speichern
//    POST /api/api.php  { "action": "vote",   "id":  123  }    → Vote +1
// ============================================================

declare(strict_types=1);

require_once __DIR__ . '/../config/db.php';

// ── Helpers ───────────────────────────────────────────────

/**
 * Ermittelt die echte Client-IP auch hinter dem Infomaniak-Reverse-Proxy.
 * Gibt eine bereinigte, auf 45 Zeichen begrenzte Zeichenkette zurück.
 */
function get_client_ip(): string
{
    $forwarded = $_SERVER['HTTP_X_FORWARDED_FOR'] ?? '';
    if ($forwarded !== '') {
        // X-Forwarded-For kann eine kommagetrennte Liste sein; erste IP = echter Client
        $ip = trim(explode(',', $forwarded)[0]);
        if (filter_var($ip, FILTER_VALIDATE_IP)) {
            return substr($ip, 0, 45);
        }
    }

    $ip = $_SERVER['REMOTE_ADDR'] ?? '0.0.0.0';
    return substr($ip, 0, 45);
}

function json_out(mixed $data, int $status = 200): never
{
    http_response_code($status);
    header('Content-Type: application/json; charset=utf-8');
    header('Access-Control-Allow-Origin: *');
    header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
    header('Access-Control-Allow-Headers: Content-Type');
    echo json_encode($data, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
    exit;
}

function request_body(): array
{
    return (array) (json_decode(file_get_contents('php://input'), true) ?? []);
}

// ── CORS preflight ────────────────────────────────────────

if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    header('Access-Control-Allow-Origin: *');
    header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
    header('Access-Control-Allow-Headers: Content-Type');
    http_response_code(204);
    exit;
}

// ── OpenAI Moderation ─────────────────────────────────────
// Gibt true zurück wenn der Text unbedenklich ist.
// Bei API-Fehler wird der Text als sicher behandelt (fail-open).

function is_safe(string $text): bool
{
    if (!defined('OPENAI_API_KEY') || OPENAI_API_KEY === 'sk-your-openai-key') {
        return true; // kein Key gesetzt → Moderation überspringen
    }

    $ch = curl_init('https://api.openai.com/v1/moderations');
    curl_setopt_array($ch, [
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_POST           => true,
        CURLOPT_POSTFIELDS     => json_encode(['input' => $text]),
        CURLOPT_HTTPHEADER     => [
            'Content-Type: application/json',
            'Authorization: Bearer ' . OPENAI_API_KEY,
        ],
        CURLOPT_TIMEOUT        => 8,
    ]);

    $raw = curl_exec($ch);
    $err = curl_error($ch);
    curl_close($ch);

    if ($err || !$raw) {
        error_log('[Polenta Quotes] OpenAI Moderation fehlgeschlagen: ' . $err);
        return true;
    }

    $body = json_decode($raw, true);
    return isset($body['results'][0]['flagged']) && $body['results'][0]['flagged'] === false;
}

// ── Route: GET (alle Quotes) ──────────────────────────────

function get_quotes(): void
{
    $pdo  = getPDO();
    $stmt = $pdo->query(
        "SELECT id, text, votes
         FROM   quotes
         WHERE  status = 'approved'
         ORDER  BY votes DESC, id ASC"
    );
    json_out($stmt->fetchAll());
}

// ── Route: GET ?winner=true ───────────────────────────────

function get_winner(): void
{
    $pdo  = getPDO();
    $stmt = $pdo->query(
        "SELECT text FROM quotes
         WHERE  status = 'approved'
         ORDER  BY votes DESC
         LIMIT  1"
    );
    $row = $stmt->fetch();
    json_out(['winner' => $row ? $row['text'] : '']);
}

// ── Route: POST submit ────────────────────────────────────

function post_submit(): void
{
    $body = request_body();
    $text = trim($body['text'] ?? '');

    if ($text === '') {
        json_out(['error' => 'Kein Text angegeben.'], 422);
    }

    if (mb_strlen($text) > 140) {
        json_out(['error' => 'Maximal 140 Zeichen erlaubt.'], 422);
    }

    $ip  = get_client_ip();
    $pdo = getPDO();

    // Rate-Limit: maximal 1 Spruch pro IP innerhalb von 60 Minuten
    $check = $pdo->prepare(
        'SELECT COUNT(*) FROM quotes
         WHERE ip_address = :ip AND created_at > NOW() - INTERVAL 1 HOUR'
    );
    $check->execute([':ip' => $ip]);
    if ((int) $check->fetchColumn() > 0) {
        json_out(['error' => 'Chill mal! Nur ein Spruch pro Stunde!'], 429);
    }

    $safe   = is_safe($text);
    $status = $safe ? 'approved' : 'pending';

    $stmt = $pdo->prepare(
        'INSERT INTO quotes (text, status, ip_address) VALUES (:text, :status, :ip)'
    );
    $stmt->execute([':text' => $text, ':status' => $status, ':ip' => $ip]);

    $message = $safe
        ? 'Dein Spruch ist live!'
        : 'Dein Spruch wird noch geprüft und bald freigeschaltet.';

    json_out(['success' => true, 'message' => $message, 'status' => $status], 201);
}

// ── Route: POST vote ──────────────────────────────────────

function post_vote(): void
{
    $body = request_body();
    $id   = filter_var($body['id'] ?? 0, FILTER_VALIDATE_INT);

    if (!$id || $id <= 0) {
        json_out(['error' => 'Ungültige ID.'], 422);
    }

    $ip  = get_client_ip();
    $pdo = getPDO();

    // UNIQUE KEY (quote_id, ip_address) auf der votes-Tabelle verhindert Doppel-Votes.
    // PDOException mit errorInfo[1] === 1062 = MySQL Duplicate Entry.
    try {
        $ins = $pdo->prepare(
            'INSERT INTO votes (quote_id, ip_address) VALUES (:quote_id, :ip)'
        );
        $ins->execute([':quote_id' => $id, ':ip' => $ip]);
    } catch (PDOException $e) {
        if ((int) ($e->errorInfo[1] ?? 0) === 1062) {
            json_out(['error' => 'Du hast für diesen Spruch schon abgestimmt.'], 403);
        }
        error_log('[Polenta Quotes] votes INSERT fehlgeschlagen: ' . $e->getMessage());
        json_out(['error' => 'Interner Fehler beim Abstimmen.'], 500);
    }

    $upd = $pdo->prepare(
        "UPDATE quotes SET votes = votes + 1 WHERE id = :id AND status = 'approved'"
    );
    $upd->execute([':id' => $id]);

    if ($upd->rowCount() === 0) {
        // Quote existiert nicht oder ist nicht freigegeben – Vote-Eintrag zurückrollen
        $del = $pdo->prepare(
            'DELETE FROM votes WHERE quote_id = :quote_id AND ip_address = :ip'
        );
        $del->execute([':quote_id' => $id, ':ip' => $ip]);
        json_out(['error' => 'Quote nicht gefunden oder nicht freigegeben.'], 404);
    }

    $row = $pdo->prepare('SELECT votes FROM quotes WHERE id = :id');
    $row->execute([':id' => $id]);
    $votes = (int) $row->fetchColumn();

    json_out(['success' => true, 'votes' => $votes]);
}

// ── Dispatch ──────────────────────────────────────────────

$method = $_SERVER['REQUEST_METHOD'];

if ($method === 'GET') {
    isset($_GET['winner']) ? get_winner() : get_quotes();
}

if ($method === 'POST') {
    $body   = request_body();
    $action = $body['action'] ?? '';

    match ($action) {
        'submit' => post_submit(),
        'vote'   => post_vote(),
        default  => json_out(['error' => 'Unbekannte Aktion.'], 400),
    };
}

json_out(['error' => 'Methode nicht erlaubt.'], 405);
