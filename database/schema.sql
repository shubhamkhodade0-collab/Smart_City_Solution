-- =====================================================================
-- Smart City Complaint Monitoring & Management System
-- PostgreSQL schema  (single authoritative data source)
-- Apply:  psql -d scms -f database/schema.sql
-- =====================================================================

DROP SCHEMA IF EXISTS public CASCADE;
CREATE SCHEMA public;

CREATE EXTENSION IF NOT EXISTS pgcrypto;   -- crypt()/gen_salt() for bcrypt password hashing

-- ---------------------------------------------------------------------
-- Geography: state -> district -> city
-- ---------------------------------------------------------------------
CREATE TABLE states (
    id          SERIAL PRIMARY KEY,
    name        VARCHAR(80)  NOT NULL UNIQUE,
    code        VARCHAR(8)   NOT NULL
);

CREATE TABLE districts (
    id          SERIAL PRIMARY KEY,
    state_id    INTEGER      NOT NULL REFERENCES states(id) ON DELETE RESTRICT,
    name        VARCHAR(80)  NOT NULL,
    CONSTRAINT uq_district_per_state UNIQUE (state_id, name)
);
CREATE INDEX idx_districts_state ON districts(state_id);

CREATE TABLE cities (
    id          SERIAL PRIMARY KEY,
    district_id INTEGER      NOT NULL REFERENCES districts(id) ON DELETE RESTRICT,
    name        VARCHAR(80)  NOT NULL,
    latitude    NUMERIC(8,5) NOT NULL CHECK (latitude  BETWEEN 5 AND 38),
    longitude   NUMERIC(8,5) NOT NULL CHECK (longitude BETWEEN 67 AND 98),
    CONSTRAINT uq_city_per_district UNIQUE (district_id, name)
);
CREATE INDEX idx_cities_district ON cities(district_id);
CREATE INDEX idx_cities_name_lower ON cities(LOWER(name));

-- ---------------------------------------------------------------------
-- Organisation
-- ---------------------------------------------------------------------
CREATE TABLE departments (
    id          SERIAL PRIMARY KEY,
    code        VARCHAR(16)  NOT NULL UNIQUE,
    name        VARCHAR(100) NOT NULL UNIQUE,
    description TEXT         NOT NULL DEFAULT ''
);

CREATE TABLE categories (
    id            SERIAL PRIMARY KEY,
    code          VARCHAR(16)  NOT NULL UNIQUE,
    name          VARCHAR(60)  NOT NULL UNIQUE,
    asm_code      SMALLINT     NOT NULL UNIQUE CHECK (asm_code BETWEEN 0 AND 7),  -- index into the Assembly weight table
    department_id INTEGER      NOT NULL REFERENCES departments(id),
    sla_hours     INTEGER      NOT NULL CHECK (sla_hours > 0)
);

CREATE TABLE users (
    id            SERIAL PRIMARY KEY,
    username      VARCHAR(40)  NOT NULL UNIQUE CHECK (username ~ '^[a-z0-9._]{3,40}$'),
    password_hash TEXT         NOT NULL CHECK (password_hash LIKE '$2%'),          -- bcrypt only, never plaintext
    full_name     VARCHAR(100) NOT NULL CHECK (LENGTH(TRIM(full_name)) >= 2),
    phone         VARCHAR(15)  NOT NULL CHECK (phone ~ '^[6-9][0-9]{9}$'),
    role          VARCHAR(12)  NOT NULL CHECK (role IN ('CITIZEN','DEPARTMENT','ADMIN')),
    department_id INTEGER      REFERENCES departments(id),
    city_id       INTEGER      REFERENCES cities(id),
    is_active     BOOLEAN      NOT NULL DEFAULT TRUE,
    created_at    TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    last_login_at TIMESTAMPTZ,
    CONSTRAINT ck_department_user CHECK (
        (role = 'DEPARTMENT' AND department_id IS NOT NULL) OR
        (role <> 'DEPARTMENT' AND department_id IS NULL))
);
CREATE INDEX idx_users_role ON users(role);

-- ---------------------------------------------------------------------
-- Complaints  (complaint -> city_id -> district_id -> state_id)
-- ---------------------------------------------------------------------
CREATE TABLE complaints (
    id              BIGSERIAL    PRIMARY KEY,
    reference_code  VARCHAR(24)  NOT NULL UNIQUE,
    citizen_id      INTEGER      NOT NULL REFERENCES users(id),
    citizen_name    VARCHAR(100) NOT NULL,
    phone           VARCHAR(15)  NOT NULL CHECK (phone ~ '^[6-9][0-9]{9}$'),
    city_id         INTEGER      NOT NULL REFERENCES cities(id),
    locality        VARCHAR(120) NOT NULL CHECK (LENGTH(TRIM(locality)) >= 3),
    category_id     INTEGER      NOT NULL REFERENCES categories(id),
    description     TEXT         NOT NULL CHECK (LENGTH(TRIM(description)) >= 15),
    impact          SMALLINT     NOT NULL CHECK (impact  BETWEEN 1 AND 5),
    urgency         SMALLINT     NOT NULL CHECK (urgency BETWEEN 1 AND 5),
    severity_score  SMALLINT     NOT NULL CHECK (severity_score BETWEEN 0 AND 100),
    severity_level  VARCHAR(10)  NOT NULL CHECK (severity_level IN ('LOW','MEDIUM','HIGH','CRITICAL')),
    priority        VARCHAR(10)  NOT NULL CHECK (priority IN ('LOW','NORMAL','HIGH','URGENT')),
    status          VARCHAR(12)  NOT NULL DEFAULT 'PENDING'
                    CHECK (status IN ('PENDING','ASSIGNED','IN_PROGRESS','RESOLVED','CLOSED')),
    department_id   INTEGER      REFERENCES departments(id),
    created_at      TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    updated_at      TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    resolved_at     TIMESTAMPTZ,
    closed_at       TIMESTAMPTZ,
    CONSTRAINT ck_assigned_has_department CHECK (status = 'PENDING' OR department_id IS NOT NULL),
    CONSTRAINT ck_resolved_time CHECK (resolved_at IS NULL OR resolved_at >= created_at)
);
CREATE INDEX idx_complaints_status     ON complaints(status);
CREATE INDEX idx_complaints_city       ON complaints(city_id);
CREATE INDEX idx_complaints_category   ON complaints(category_id);
CREATE INDEX idx_complaints_department ON complaints(department_id);
CREATE INDEX idx_complaints_citizen    ON complaints(citizen_id);
CREATE INDEX idx_complaints_created    ON complaints(created_at);
CREATE INDEX idx_complaints_severity   ON complaints(severity_level);

CREATE SEQUENCE complaint_reference_seq START 1001;

-- general event timeline (created, assigned, notes, ...)
CREATE TABLE complaint_history (
    id           BIGSERIAL   PRIMARY KEY,
    complaint_id BIGINT      NOT NULL REFERENCES complaints(id) ON DELETE CASCADE,
    event_type   VARCHAR(20) NOT NULL CHECK (event_type IN ('CREATED','ASSIGNED','STATUS_CHANGED','RESOLVED','CLOSED','NOTE')),
    details      TEXT        NOT NULL,
    actor_id     INTEGER     REFERENCES users(id),
    created_at   TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
CREATE INDEX idx_history_complaint ON complaint_history(complaint_id, created_at);

-- strict lifecycle trail: one row per status change
CREATE TABLE complaint_status_history (
    id           BIGSERIAL   PRIMARY KEY,
    complaint_id BIGINT      NOT NULL REFERENCES complaints(id) ON DELETE CASCADE,
    old_status   VARCHAR(12) CHECK (old_status IN ('PENDING','ASSIGNED','IN_PROGRESS','RESOLVED','CLOSED')),
    new_status   VARCHAR(12) NOT NULL CHECK (new_status IN ('PENDING','ASSIGNED','IN_PROGRESS','RESOLVED','CLOSED')),
    changed_by   INTEGER     REFERENCES users(id),
    note         TEXT        NOT NULL DEFAULT '',
    changed_at   TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    CONSTRAINT ck_valid_transition CHECK (
        (old_status IS NULL          AND new_status = 'PENDING')     OR
        (old_status = 'PENDING'      AND new_status = 'ASSIGNED')    OR
        (old_status = 'ASSIGNED'     AND new_status = 'IN_PROGRESS') OR
        (old_status = 'IN_PROGRESS'  AND new_status = 'RESOLVED')    OR
        (old_status = 'RESOLVED'     AND new_status = 'CLOSED'))
);
CREATE INDEX idx_status_history_complaint ON complaint_status_history(complaint_id, changed_at);

CREATE TABLE notifications (
    id           BIGSERIAL    PRIMARY KEY,
    user_id      INTEGER      NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    complaint_id BIGINT       REFERENCES complaints(id) ON DELETE CASCADE,
    type         VARCHAR(24)  NOT NULL CHECK (type IN ('COMPLAINT_CREATED','COMPLAINT_ASSIGNED','STATUS_CHANGED',
                                                       'COMPLAINT_RESOLVED','COMPLAINT_CLOSED','ADMIN_ACTION','DEPARTMENT_ASSIGNMENT')),
    title        VARCHAR(120) NOT NULL,
    message      TEXT         NOT NULL,
    is_read      BOOLEAN      NOT NULL DEFAULT FALSE,
    created_at   TIMESTAMPTZ  NOT NULL DEFAULT NOW()
);
CREATE INDEX idx_notifications_user ON notifications(user_id, is_read, created_at DESC);

CREATE TABLE audit_logs (
    id          BIGSERIAL   PRIMARY KEY,
    user_id     INTEGER     REFERENCES users(id),
    action      VARCHAR(30) NOT NULL CHECK (action IN ('LOGIN','LOGOUT','LOGIN_FAILED','REGISTER','COMPLAINT_CREATED',
                                                       'COMPLAINT_ASSIGNED','STATUS_CHANGED','COMPLAINT_RESOLVED',
                                                       'COMPLAINT_CLOSED','USER_CREATED','ADMIN_ACTION','REPORT_EXPORTED')),
    entity_type VARCHAR(20) NOT NULL DEFAULT '',
    entity_id   VARCHAR(40) NOT NULL DEFAULT '',
    details     TEXT        NOT NULL DEFAULT '',
    created_at  TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
CREATE INDEX idx_audit_created ON audit_logs(created_at DESC);

CREATE TABLE complaint_attachments (
    id           BIGSERIAL    PRIMARY KEY,
    complaint_id BIGINT       NOT NULL REFERENCES complaints(id) ON DELETE CASCADE,
    file_name    VARCHAR(200) NOT NULL,
    mime_type    VARCHAR(60)  NOT NULL,
    size_bytes   INTEGER      NOT NULL CHECK (size_bytes > 0 AND size_bytes <= 10485760),
    uploaded_at  TIMESTAMPTZ  NOT NULL DEFAULT NOW()
);

-- convenience view used by every complaint query: resolves city -> district -> state
CREATE VIEW v_complaints AS
SELECT c.*, ci.name AS city_name, d.id AS district_id, d.name AS district_name,
       s.id AS state_id, s.name AS state_name, cat.code AS category_code, cat.name AS category_name,
       cat.asm_code, dep.name AS department_name, dep.code AS department_code
FROM complaints c
JOIN cities ci     ON ci.id = c.city_id
JOIN districts d   ON d.id = ci.district_id
JOIN states s      ON s.id = d.state_id
JOIN categories cat ON cat.id = c.category_id
LEFT JOIN departments dep ON dep.id = c.department_id;
