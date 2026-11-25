-- =====================================================================
-- Reference seed data (run AFTER schema.sql and seed_geo.sql)
-- Passwords are hashed by PostgreSQL pgcrypto bcrypt; plaintext is never stored.
-- Default credentials are documented in README.md (change them after first login).
-- =====================================================================

INSERT INTO departments (code, name, description) VALUES
  ('TRAFFIC',   'Traffic Police & Transport',      'Signals, congestion, illegal parking, road safety enforcement'),
  ('SANITATION','Solid Waste Management',          'Garbage collection, overflowing bins, street sweeping'),
  ('POWER',     'Electricity Distribution',        'Power cuts, exposed wiring, transformer faults, street lights'),
  ('WATER',     'Water Supply & Sewerage',         'No supply, contamination, pipeline leaks, sewer overflow'),
  ('PWD',       'Public Works Department (Roads)', 'Potholes, damaged roads, footpaths, bridges'),
  ('DRAINAGE',  'Storm Water Drainage',            'Blocked drains, waterlogging, flooding');

-- asm_code is the index into the weight table inside assembly/severity.asm
INSERT INTO categories (code, name, asm_code, department_id, sla_hours) VALUES
  ('TRAFFIC',     'Traffic',     0, (SELECT id FROM departments WHERE code='TRAFFIC'),    24),
  ('GARBAGE',     'Garbage',     1, (SELECT id FROM departments WHERE code='SANITATION'), 48),
  ('ELECTRICITY', 'Electricity', 2, (SELECT id FROM departments WHERE code='POWER'),      12),
  ('WATER',       'Water',       3, (SELECT id FROM departments WHERE code='WATER'),      24),
  ('ROADS',       'Roads',       4, (SELECT id FROM departments WHERE code='PWD'),        72),
  ('DRAINAGE',    'Drainage',    5, (SELECT id FROM departments WHERE code='DRAINAGE'),   36);

-- Administrator and one officer per department
INSERT INTO users (username, password_hash, full_name, phone, role, department_id) VALUES
  ('admin',              crypt('Admin@123', gen_salt('bf', 8)), 'System Administrator', '9800000001', 'ADMIN', NULL),
  ('traffic.officer',    crypt('Dept@123',  gen_salt('bf', 8)), 'Traffic Control Officer',   '9800000011', 'DEPARTMENT', (SELECT id FROM departments WHERE code='TRAFFIC')),
  ('sanitation.officer', crypt('Dept@123',  gen_salt('bf', 8)), 'Sanitation Officer',        '9800000012', 'DEPARTMENT', (SELECT id FROM departments WHERE code='SANITATION')),
  ('power.officer',      crypt('Dept@123',  gen_salt('bf', 8)), 'Electricity Officer',       '9800000013', 'DEPARTMENT', (SELECT id FROM departments WHERE code='POWER')),
  ('water.officer',      crypt('Dept@123',  gen_salt('bf', 8)), 'Water Supply Officer',      '9800000014', 'DEPARTMENT', (SELECT id FROM departments WHERE code='WATER')),
  ('pwd.officer',        crypt('Dept@123',  gen_salt('bf', 8)), 'Roads (PWD) Officer',       '9800000015', 'DEPARTMENT', (SELECT id FROM departments WHERE code='PWD')),
  ('drainage.officer',   crypt('Dept@123',  gen_salt('bf', 8)), 'Drainage Officer',          '9800000016', 'DEPARTMENT', (SELECT id FROM departments WHERE code='DRAINAGE'));

-- One real citizen account for testing the citizen portal
INSERT INTO users (username, password_hash, full_name, phone, role, city_id) VALUES
  ('citizen', crypt('Citizen@123', gen_salt('bf', 8)), 'Test Citizen', '9876543210', 'CITIZEN',
   (SELECT ci.id FROM cities ci JOIN districts d ON d.id = ci.district_id JOIN states s ON s.id = d.state_id
     WHERE s.name = 'Maharashtra' AND ci.name = 'Pune'));
