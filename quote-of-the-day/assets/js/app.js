// ============================================================
//  Polenta Quotes — app.js
//
//  Kommuniziert mit api/api.php.
//  Tabellenspalten: id | text | votes | status
// ============================================================

const API = 'api/api.php';

// ── DOM refs ──────────────────────────────────────────────
const submitForm      = document.getElementById('submitForm');
const quoteInput      = document.getElementById('quoteInput');
const charCount       = document.getElementById('charCount');
const submitBtn       = document.getElementById('submitBtn');
const quotesContainer = document.getElementById('quotes-container');
const skeleton        = document.getElementById('skeleton');
const toast           = document.getElementById('toast');

// ── Toast ─────────────────────────────────────────────────

let toastTimer = null;

function showToast(message, type = 'success') {
    toast.className   = type;   // 'success' | 'error' | 'warning' | 'info'
    toast.textContent = message;
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => toast.classList.add('hidden'), 3800);
}

// ── Zeichenzähler ─────────────────────────────────────────

quoteInput.addEventListener('input', () => {
    const len = quoteInput.value.length;
    charCount.textContent = `${len} / 140`;
    charCount.classList.toggle('danger', len > 120);
});

// ── XSS-Schutz ────────────────────────────────────────────

function escapeHtml(str) {
    return str
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;');
}

// ── Render ────────────────────────────────────────────────

/**
 * Baut die Quote-Karten und hängt sie in #quotes-container.
 * @param {Array<{id: number, text: string, votes: number}>} quotes
 */
function renderQuotes(quotes) {
    skeleton.style.display        = 'none';
    quotesContainer.style.display = 'flex';
    quotesContainer.innerHTML     = '';

    if (!quotes.length) {
        quotesContainer.innerHTML = `
            <div class="empty-state">
                <p>Noch keine Sprüche &mdash; sei der Erste.</p>
            </div>`;
        return;
    }

    quotes.forEach((q, i) => {
        const card = document.createElement('div');
        card.className  = 'quote-card';
        card.dataset.id = q.id;

        card.innerHTML = `
            <div class="quote-rank${i < 3 ? ' top' : ''}" aria-hidden="true">#${i + 1}</div>
            <div class="quote-body">
                <p class="quote-text">${escapeHtml(q.text)}</p>
            </div>
            <div class="quote-vote-col">
                <button class="vote-btn" data-id="${q.id}" aria-label="Abstimmen">+</button>
                <span class="vote-count">${q.votes}</span>
            </div>`;

        quotesContainer.appendChild(card);
    });

    quotesContainer.querySelectorAll('.vote-btn').forEach(btn => {
        btn.addEventListener('click', () => handleVote(btn));
    });
}

// ── Quotes laden ──────────────────────────────────────────

async function loadQuotes() {
    skeleton.style.display        = 'flex';
    skeleton.style.flexDirection  = 'column';
    quotesContainer.style.display = 'none';

    try {
        const res = await fetch(API, { cache: 'no-store' });
        if (!res.ok) throw new Error(`HTTP ${res.status}`);

        const quotes = await res.json();
        renderQuotes(Array.isArray(quotes) ? quotes : []);

    } catch (err) {
        skeleton.style.display        = 'none';
        quotesContainer.style.display = 'flex';
        quotesContainer.innerHTML = `
            <div class="error-banner">
                <p>Sprüche konnten nicht geladen werden. Bitte Seite neu laden.</p>
            </div>`;
        console.error('[loadQuotes]', err);
    }
}

// ── Submit ────────────────────────────────────────────────

submitForm.addEventListener('submit', async (e) => {
    e.preventDefault();

    const text = quoteInput.value.trim();

    if (!text) {
        showToast('Gib etwas ein!', 'warning');
        quoteInput.focus();
        return;
    }

    const label        = submitBtn.textContent;
    submitBtn.disabled = true;
    submitBtn.textContent = 'Wird gesendet …';

    try {
        const res = await fetch(API, {
            method:  'POST',
            headers: { 'Content-Type': 'application/json' },
            body:    JSON.stringify({ action: 'submit', text }),
        });

        const data = await res.json();

        if (!res.ok) {
            showToast(data.error ?? 'Fehler beim Einreichen.', 'error');
            return;
        }

        showToast(data.message ?? 'Spruch eingereicht!', 'success');
        quoteInput.value      = '';
        charCount.textContent = '0 / 140';
        charCount.classList.remove('danger');

        if (data.status === 'approved') {
            await loadQuotes();
        }

    } catch {
        showToast('Netzwerkfehler. Bitte erneut versuchen.', 'error');
    } finally {
        submitBtn.disabled    = false;
        submitBtn.textContent = label;
    }
});

// ── Vote ──────────────────────────────────────────────────

async function handleVote(btn) {
    const id = parseInt(btn.dataset.id, 10);
    if (!id) return;

    btn.disabled = true;

    try {
        const res = await fetch(API, {
            method:  'POST',
            headers: { 'Content-Type': 'application/json' },
            body:    JSON.stringify({ action: 'vote', id }),
        });

        const data = await res.json();

        if (!res.ok) {
            showToast(data.error ?? 'Fehler beim Abstimmen.', 'error');
            if (res.status === 403) {
                // Bereits gevoted – Button dauerhaft als voted markieren
                btn.classList.add('voted');
                btn.textContent = '✓';
            } else {
                btn.disabled = false;
            }
            return;
        }

        btn.classList.add('voted');
        btn.textContent = '✓';
        btn.closest('.quote-card').querySelector('.vote-count').textContent = data.votes;

        setTimeout(loadQuotes, 700);

    } catch {
        showToast('Netzwerkfehler beim Abstimmen.', 'error');
        btn.disabled = false;
    }
}

// ── Init ──────────────────────────────────────────────────

loadQuotes();
