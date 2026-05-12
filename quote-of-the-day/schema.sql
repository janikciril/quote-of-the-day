-- ============================================================
--  Polenta Quotes – Datenbankschema
--  Einmalig in phpMyAdmin auf dem Infomaniak-Server ausführen.
--  Datenbank-Name muss mit DB_NAME in config/db.php übereinstimmen.
-- ============================================================

CREATE TABLE IF NOT EXISTS `quotes` (
    `id`         INT UNSIGNED                   NOT NULL AUTO_INCREMENT,
    `text`       VARCHAR(140)                   NOT NULL,
    `votes`      INT UNSIGNED                   NOT NULL DEFAULT 0,
    `status`     ENUM('pending', 'approved')    NOT NULL DEFAULT 'pending',
    `ip_address` VARCHAR(45)                    DEFAULT NULL,
    `created_at` DATETIME                       NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    INDEX `idx_status_votes`  (`status`, `votes` DESC),
    INDEX `idx_ip_created_at` (`ip_address`, `created_at`)
) ENGINE = InnoDB
  DEFAULT CHARSET = utf8mb4
  COLLATE = utf8mb4_unicode_ci;

-- ============================================================
--  Migration: Bestehende Tabelle erweitern (falls bereits vorhanden)
--  Nur ausführen wenn die Tabelle schon existiert und die Spalten fehlen.
-- ============================================================

ALTER TABLE `quotes`
    ADD COLUMN IF NOT EXISTS `ip_address` VARCHAR(45) DEFAULT NULL AFTER `status`,
    ADD COLUMN IF NOT EXISTS `created_at` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP AFTER `ip_address`,
    ADD INDEX  IF NOT EXISTS `idx_ip_created_at` (`ip_address`, `created_at`);
