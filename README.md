==========Link to get Groq api key============
https://console.groq.com/keys
==================env setup===================
find the file .env.example and copy the contents in .env file by creating it
Then paste your groq key in .env file before building application


----------Commands to run in terminal---------
..............................................

==========To Build and run application========
make clean
make frontend
make backend
$env:Path = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:Path
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\database\db.ps1 setup
.\database\db.ps1 demo
.\build\scms_app.exe

====================To stop===================
ctrl + c in terminal where .\build\scms_app.exe was ran.
.\database\db.ps1 clear-demo -Force  -> removes all demo data


========If server stopped========
if server stopped
.\database\db.ps1 start
.\database\db.ps1 clear-demo -Force 
if server stopped
.\database\db.ps1 stop



=========To return to initial state of application========
make clean.


# Smart City Complaint Monitoring & Management System

A native desktop application for reporting, routing, tracking and analysing civic complaints across India.

| Layer | Technology |
|-------|------------|
| Frontend | C++17 + OpenGL / FreeGLUT (custom widget toolkit, no web technology) |
| Backend | C++17 (OOP services, hand-written data structures) |
| Assembly engine | x86-64 NASM (severity score, string routines) |
| Database | PostgreSQL 18 |
| AI assistant | Local rule-based expert system in C++ (no internet, no API key) |

There are three portals: **Citizen**, **Department** and **Administrator**.

## What is new in this version

**Latest update**

* Password fields now include an eye control to show or hide the password while entering it.
* **Paste in text fields:** press `Ctrl+V` after focusing any text field, including login usernames and the Groq API key field.
* **PostgreSQL 18.** The project no longer uses PostgreSQL 17. The old `database\pgdata` folder (a PostgreSQL 17 cluster) is not shipped any more; `db.ps1 setup` / `setup_db.sh` create a PostgreSQL 18 cluster and move any older folder aside instead of failing.
* **VS Code PowerShell commands** (`database\db.ps1`) to start, stop and connect to the database and to **add and delete demo data**; plain-PowerShell equivalents are in the README. New CLI commands `--demo-status`, `--clear-demo [--users]`.
* **Direct AI.** Add your Groq key to the project-root `.env` file and the assistant answers **any** question, not only complaint topics.
* **Departments screen:** KPI strip, sorting, officer on each card, a **detail dialog** (officers, SLA metrics, most urgent complaints, busiest cities, AI insight) and CSV export.
* All **department officer logins** are listed in [Login accounts](#login-accounts).

* **Optional Groq AI** - configure your Groq API key in `.env` and the assistant also answers questions the built-in playbooks cannot (see [Optional: connect Groq](#optional-connect-groq)).
* **Fixed - District / City lists on the Create-account page (and the Report form) were empty.** The dropdown widget lost its "selection changed" signal.
* **Fixed - the Apply button covered the sort options** on All Complaints / Work Queue / My Complaints at about 1366 px screen width.
* **Fixed - hidden table columns** (Location and Department on the complaint lists, Full name / Department on Users, the first column of Reports) were 1-2 pixels wide.
* **Fixed - small screens:** the admin menu no longer hides *System & Settings* behind the user card, and the layout no longer leaves a blank strip when the window is smaller than 1280 x 760.

**Earlier in this version**

* **India Map and City Ranking are now in the Citizen and Department portals** (before, only the administrator had them). Citizens and departments can switch between *All India* (aggregate counts only, no personal data) and *My complaints* / *My department*.
* **AI Assistant** for citizens and departments (details in [AI assistant](#ai-assistant)):
  * a chat screen with a live **AI triage** list,
  * an **AI card** on every complaint (fix plan for departments, "what happens next / stay safe" for citizens, ready-to-use status notes),
  * an **AI writing helper** on the *Report complaint* form (suggests category, impact and urgency).
* **Fixed:** the map's `+`, `-` and `Reset view` buttons did nothing (the map's drag handler swallowed the click).
* **Fixed:** the *State* column of the City Ranking table was 1 pixel wide.
* **Fixed:** the city bubbles on the map were cached for the whole program run; they now follow the data and the selected scope.
* `make all` no longer fails when `tests/test_main.cpp` is missing; new `make test-ai` and `make test-llm` targets.

---

## Contents

1. [Requirements](#requirements)
2. [Quick start](#quick-start)
3. [Build: backend and frontend](#build-backend-and-frontend)
4. [VS Code terminal: database commands](#vs-code-terminal-database-commands)
5. [Run](#run)
6. [Login accounts](#login-accounts)
7. [Map and City Ranking in every portal](#map-and-city-ranking-in-every-portal)
   * [Departments screen](#departments-screen-administrator)
8. [AI assistant](#ai-assistant)
   * [Optional: connect Groq](#optional-connect-groq)
9. [Command line (backend) tools](#command-line-backend-tools)
10. [Tests](#tests)
11. [Project layout](#project-layout)
12. [Troubleshooting](#troubleshooting)

---

## Requirements

* Windows 10 / 11 (the fonts in `config/app.conf` are Windows fonts).
* [MSYS2](https://www.msys2.org/) in `C:\msys64` (compiler, `make`, FreeGLUT, NASM).
* **PostgreSQL 18** (tested design target: **18.6**). Either the MSYS2 package or the normal Windows installer works; the project's own database server runs from the folder `database\pgdata` on **port 5433**, so it never clashes with another PostgreSQL on 5432.

> **PostgreSQL 17 is no longer used.** A PostgreSQL data folder can only be opened by the *same major version* that created it, so a `database\pgdata` folder made by PostgreSQL 17 cannot be started by PostgreSQL 18. This version of the project does **not** ship a data folder; `db.ps1 setup` (or `setup_db.sh`) creates a fresh PostgreSQL 18 cluster, and if it finds an old folder it moves it aside to `database\pgdata_pg17_backup` (never deletes it).

Install the toolchain once, from the **MSYS2 UCRT64** shell:

```bash
pacman -Syu
pacman -S --needed make mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-nasm \
                   mingw-w64-ucrt-x86_64-freeglut mingw-w64-ucrt-x86_64-postgresql
```

This gives you `g++`, `nasm`, FreeGLUT, `libpq` and the PostgreSQL server tools (`initdb`, `pg_ctl`, `psql`). If you installed PostgreSQL 18 with the **EnterpriseDB Windows installer** instead, nothing else is needed: the helper script finds `C:\Program Files\PostgreSQL\18\bin` by itself.

Check which version you have:

```powershell
$env:Path = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:Path
pg_ctl --version          # pg_ctl (PostgreSQL) 18.6
psql --version
```

---

## Quick start

Open the project folder in **VS Code** (`File > Open Folder`), then open a terminal with **Terminal > New Terminal** (it is PowerShell). Everything below is typed there.

```powershell
# 1) make the MSYS2 tools visible in this terminal (repeat in every new terminal)
$env:Path = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:Path

# 2) build backend + frontend
make backend
make frontend
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
# 3) create the PostgreSQL 18 database (first time only: cluster + tables + accounts)
.\database\db.ps1 setup

# 4) put 300 demo complaints in (optional)
.\database\db.ps1 demo

# 5) run the application
.\build\scms_app.exe
```

If PowerShell says *"running scripts is disabled on this system"*, allow scripts for this terminal only:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
```

Next day you only need `.\database\db.ps1 start` and `.\build\scms_app.exe`. All daily database commands are in [VS Code terminal: database commands](#vs-code-terminal-database-commands).

---

## Build: backend and frontend

Run these in PowerShell **from the project root** after the `$env:Path = ...` line from *Quick start*.

| What | Command | Output |
|------|---------|--------|
| **Backend** (C++ services + data structures + NASM + command-line tool) | `make backend` | `build\scms_cli.exe` |
| **Frontend** (OpenGL desktop application) | `make frontend` | `build\scms_app.exe` |
| Both | `make all` | both files above |
| Unit tests of the AI assistant | `make test-ai` | runs `build\scms_ai_tests.exe` |
| Put 300 demo complaints in the database | `make demo` | |
| Build and start the application | `make run` | |
| Start / initialise the database through the Makefile | `make database` | |
| Delete all build output | `make clean` | |

```powershell
make backend          # backend only
make frontend         # frontend only (also builds the backend objects it needs)
make -j8 all          # everything, 8 compile jobs in parallel
make clean            # start from scratch
```

Notes

* The same commands work unchanged in the MSYS2 UCRT64 shell.
* Re-run `make frontend` / `make backend` after changing any source file; `make` only recompiles what changed.
* `make all` builds the general test suite only if `tests\test_main.cpp` exists. It is not included in the archive you received, so it is skipped. `make test-ai` is independent of it.
* On Linux the Makefile uses `elf64` / `-lglut -lGLU -lGL`, but the assembly files use the **Windows x64 calling convention**, so the project is meant to run on Windows.

---

## VS Code terminal: database commands

All of these are typed in the VS Code terminal (PowerShell) from the project root. The helper script `database\db.ps1` always uses the **newest** PostgreSQL it finds (18.x), reads `config\app.conf`, and works in Windows PowerShell 5.1 and PowerShell 7.

| What you want | Command |
|---------------|---------|
| Is the server running? Which version? How many complaints? | `.\database\db.ps1 status` |
| **Start** the database server | `.\database\db.ps1 start` |
| **Stop** the database server | `.\database\db.ps1 stop` |
| **Connect** (opens `psql` as `scms_app`) | `.\database\db.ps1 connect` |
| Connect as superuser `postgres` | `.\database\db.ps1 admin` |
| Run one SQL statement | `.\database\db.ps1 query "SELECT status, COUNT(*) FROM complaints GROUP BY status;"` |
| **Add demo data** (300 complaints) | `.\database\db.ps1 demo` |
| Add another amount | `.\database\db.ps1 demo 100` |
| How many demo / real complaints? | `.\database\db.ps1 demo-status` |
| **Delete demo data** (real complaints are kept) | `.\database\db.ps1 clear-demo` |
| Delete demo data **and** the demo accounts | `.\database\db.ps1 clear-demo -Users` |
| Back the database up to `exports\` | `.\database\db.ps1 backup` |
| Create everything from scratch (**replaces** the `scms` database) | `.\database\db.ps1 setup` |
| Which PostgreSQL tools are used? | `.\database\db.ps1 version` |

`clear-demo` and `setup` ask you to type `YES`; add `-Force` to skip the question. Demo data means the complaints owned by the `demo.*` accounts (created by `demo`); complaints you enter with other accounts are never touched. Their history and notifications are removed with them. After `clear-demo -Users`, bring the 12 demo accounts back with `.\database\db.ps1 query` or `psql -f database/demo_data.sql` (see below).

A typical session:

```powershell
$env:Path = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:Path
.\database\db.ps1 start
.\database\db.ps1 demo                       # add demo data
.\build\scms_app.exe                         # ... use / present the application ...
.\database\db.ps1 clear-demo -Force          # remove the demo data again
.\database\db.ps1 stop
```

### The same commands without the script (plain PowerShell)

Values from `config\app.conf`: host `localhost`, port `5433`, database `scms`, user `scms_app`, password `scms_app_pw`; superuser `postgres` / `postgres`.

```powershell
# start / status / stop the project's server
pg_ctl -D database/pgdata -o "-p 5433" -l database/postgres.log -w start
pg_ctl -D database/pgdata status
pg_ctl -D database/pgdata -m fast stop

# connect (interactive psql: \dt tables, \d complaints, \x expanded output, \q quit)
$env:PGPASSWORD = "scms_app_pw"
psql -h localhost -p 5433 -U scms_app -d scms

# add demo data / look at it / delete it
.\build\scms_cli.exe --seed-demo 300
.\build\scms_cli.exe --demo-status
.\build\scms_cli.exe --clear-demo              # add --users to remove the demo accounts too

# the same deletion in plain SQL (history and notifications go with the complaints)
psql -h localhost -p 5433 -U scms_app -d scms -c "DELETE FROM audit_logs WHERE entity_type = 'COMPLAINT' AND entity_id IN (SELECT reference_code FROM complaints WHERE citizen_id IN (SELECT id FROM users WHERE username LIKE 'demo.%'));"
psql -h localhost -p 5433 -U scms_app -d scms -c "DELETE FROM complaints WHERE citizen_id IN (SELECT id FROM users WHERE username LIKE 'demo.%');"

# restore the 12 demo accounts after clear-demo -Users (only works when they do not exist)
psql -h localhost -p 5433 -U scms_app -d scms -v ON_ERROR_STOP=1 -f database/demo_data.sql
```

### If you use the PostgreSQL 18 Windows *service* (EnterpriseDB installer, port 5432)

The project can also use the server that the installer set up. Create the role and database once with your own `postgres` password, and tell the application the port:

```powershell
$env:Path = "C:\Program Files\PostgreSQL\18\bin;" + $env:Path
psql -h localhost -p 5432 -U postgres -c "CREATE ROLE scms_app LOGIN PASSWORD 'scms_app_pw';"
psql -h localhost -p 5432 -U postgres -c "CREATE DATABASE scms OWNER scms_app;"
psql -h localhost -p 5432 -U postgres -d scms -c "CREATE EXTENSION IF NOT EXISTS pgcrypto;"
foreach ($f in "schema", "seed_geo", "seed", "demo_data") { psql -h localhost -p 5432 -U scms_app -d scms -v ON_ERROR_STOP=1 -q -f "database/$f.sql" }

$env:SCMS_DB_PORT = "5432"          # or change DB_PORT in config\app.conf
.\build\scms_app.exe

Get-Service postgresql*             # find the service name
Stop-Service  postgresql-x64-18     # stop the server
Start-Service postgresql-x64-18     # start it again
```

### Moving to PostgreSQL 18 from an old PostgreSQL 17 data folder

* The demo complaints in an old 17 folder are not worth migrating: run `.\database\db.ps1 setup` and then `.\database\db.ps1 demo`.
* If you entered **real data** into the old PostgreSQL 17 database, dump it *with the PostgreSQL 17 tools before uninstalling them* (`pg_dump -h localhost -p 5433 -U scms_app -d scms -f old.sql`) and load it into the new database with `psql ... -f old.sql`.
* `db.ps1 setup` and `setup_db.sh` never delete the old folder; they rename it to `database\pgdata_pg17_backup`. Delete that folder yourself when you no longer need it.

### Use a different port or password

The application reads `config\app.conf`. Any value can also be overridden with an environment variable named `SCMS_` + the key:

```powershell
$env:SCMS_DB_PORT = "5432"
$env:SCMS_DB_PASSWORD = "my_password"
.\build\scms_app.exe
```

> PowerShell tip: the SQL in `-c "..."` must not contain `$` characters (PowerShell would expand them). For longer SQL use a `.sql` file and `-f`.

---

## Run

```powershell
.\build\scms_app.exe          # the application (needs the database running)
.\build\scms_cli.exe          # command-line tool, see below
```

Start the application **from the project root** (`config\app.conf` and the `exports` folder are looked up relative to it). The login screen shows `PostgreSQL is not reachable` if the server is not running; start it and press **Retry**.

---

## Login accounts

All accounts are created by `database/seed.sql` and `database/demo_data.sql` (loaded by `db.ps1 setup`).

**Administrator**

| Username | Password | Name |
|----------|----------|------|
| `admin` | `Admin@123` | System Administrator |

**Department officers** (all use the password `Dept@123`)

| Username | Password | Officer | Department | Handles |
|----------|----------|---------|------------|---------|
| `traffic.officer` | `Dept@123` | Traffic Control Officer | Traffic Police & Transport | Traffic |
| `sanitation.officer` | `Dept@123` | Sanitation Officer | Solid Waste Management | Garbage |
| `power.officer` | `Dept@123` | Electricity Officer | Electricity Distribution | Electricity |
| `water.officer` | `Dept@123` | Water Supply Officer | Water Supply & Sewerage | Water |
| `pwd.officer` | `Dept@123` | Roads (PWD) Officer | Public Works Department (Roads) | Roads |
| `drainage.officer` | `Dept@123` | Drainage Officer | Storm Water Drainage | Drainage |

**Citizens**

| Username | Password | Notes |
|----------|----------|-------|
| `citizen` | `Citizen@123` | empty test account |
| `demo.<first>.<last>` (12 accounts, for example `demo.meera.joshi`, `demo.arjun.nair`) | `Demo@123` | own the complaints created by `demo` |

Anyone can also create a citizen account from the login screen (**Create an account**). A department officer sees only the complaints assigned to their department; a citizen sees only their own; the administrator sees everything. Change these passwords before using the system for anything real.

---

## Map and City Ranking in every portal

**Where:** *Citizen* and *Department* portals have a new **INSIGHTS** section in the sidebar with **India Map**, **City Ranking** and **AI Assistant**. The administrator keeps the same screens as before.

**India Map**

* Click a state to see its districts, then a district for its cities. Zoom in to see city bubbles.
* Controls: **+** and **-** zoom, **Reset view** returns to the full map, the mouse wheel zooms, drag to pan.
* Citizens and departments get an **All India / My complaints (or My department)** switch above the search box. *All India* shows aggregate totals for everyone; the other option counts only your own complaints (citizen) or your department's complaints (department). Administrators always see all of India.

**City Ranking**

* Cities ranked by number of complaints with Quick Sort, searchable with Binary Search, Top 10 / Top 20 / All, ascending or descending. Same All India / My scope switch for citizens and departments.

Only counts are shared in the *All India* view. Nobody sees another citizen's complaint details through these screens.

---

## Departments screen (administrator)

**Administrator > Departments** now shows more than the old ranked cards:

* **KPI strip:** number of departments, open work, complaints **past their SLA**, average resolution rate, the busiest department.
* **Sort by** open workload, resolution rate, past SLA or high severity (the project's own Merge Sort; the number of comparisons is shown).
* **Cards** with the four status counts, resolution rate, *Past SLA* and *Open high-severity* counts, and the **officer in charge**.
* **Click a card** for the **detail dialog**: officers (username, phone, last login, active), a status breakdown bar, workload metrics (open, past SLA, close to SLA, critical by AI priority, average and oldest age), the **most urgent open complaints ranked by AI priority** (click one to open it), the **busiest cities**, and an **AI insight** in plain sentences. **View complaints** jumps to the complaint list filtered to that department's category.
* **Export CSV** writes one row per department to `exports\departments_<time>.csv`.

---

## AI assistant

The assistant is available to **citizens and departments** (not the administrator portal). It is written entirely in C++ and drawn with the same OpenGL widgets as the rest of the application. No HTML, CSS, JavaScript or React is used.

### What it is (and is not)

It is a **local expert system**: a curated knowledge base of 29 municipal playbooks (6 categories, several sub-issues each) + keyword scoring + live facts from PostgreSQL (status, SLA clock, similar open complaints). By default it runs entirely on your computer, needs **no internet and no API key**, and sends no data anywhere; the same complaint always gets the same advice. Only if you add a Groq key (next section) are unanswerable questions sent to Groq.

The built-in engine is **not** a large language model: it understands the topics it has playbooks for (traffic, garbage, electricity, water, roads, drainage) and will say so when it cannot tell. Its advice is general good practice, not a replacement for official procedures or emergency services.

### Where to find it

| Where | What you get |
|-------|--------------|
| **AI Assistant** (sidebar) | Chat + a live **AI triage** list on the right (your open complaints ranked by AI priority, sorted with the project's Merge Sort). Click a row to open the complaint, or **Ask** to discuss it. Suggestion chips under each answer are clickable. |
| **Complaint details** → *AI assistant* card | AI priority (0-100) with the reasons behind it, SLA clock, and: **department** - a status-aware fix plan (start now / fix / checks before resolving), resources, likely causes, repeat-pattern warnings, and a drafted **note for the citizen** with a *Use this note* button; **citizen** - what happens next, how to stay safe while waiting, how to escalate when the target time has passed. |
| **Report complaint** → *AI writing helper* | While you type, it reads the description and suggests the **category**, **impact** and **urgency** (with reasons) and what details to add. *Use category* / *Apply impact and urgency* fill the form for you. |
| Dashboards | **Ask AI assistant** (citizen) and **AI triage** (department) shortcuts. |

### How the AI priority works

`AI priority = NASM severity score + SLA pressure + risk cues + repeats + citizen priority`, capped at 100.

* The NASM severity score is never lowered, so the AI can never contradict the existing severity band (Low < 30, Medium 30-54, High 55-79, Critical 80+).
* **SLA pressure** grows from half of the category's target time until it is overdue.
* **Risk cues** in the description: children / school / hospital / elderly, injury or fire or shock words, health risks, how long it has lasted, how many people.
* **Similar open complaints** in the same city and category (a repeat pattern), and the priority the citizen chose.

Every contributing reason is listed on the complaint, so you can see *why* a number is what it is.

### Things you can ask

Department: *What should I handle first?* · *Summarise my workload* · *SCMS-2026-000126 what should I do next?* · *Draft a note for the citizen* · *How much time is left?* · *Any similar complaints?* · *Which cities have the most complaints?* · *How is severity calculated?*

Citizen: *Which of my complaints needs attention?* · *What happens next?* · *Is it safe?* · *How long will it take?* · *How do I escalate?* · *Can I close it now?* · *How do I write a good complaint?* · *What do the statuses mean?*

Type a complaint reference such as `SCMS-2026-000123` to talk about that complaint. If someone is in danger, it tells you to call **112** (fire: **101**) first.

### Optional: connect Groq (direct AI, any topic)

The built-in assistant only knows its playbooks. If you add **your own Groq API key**, the assistant becomes a **direct AI chat**: every message goes to a Groq-hosted model, so you can ask **anything** (general knowledge, study help, writing, maths, programming), not only questions about complaints. It still knows your complaints: when you discuss a complaint, or ask about priorities, status or SLA, the facts computed on your computer are passed along. This is **off by default**: nothing is sent anywhere until you add a key.

**1. Get a key** (free tier available): <https://console.groq.com/keys> > create an API key (it starts with `gsk_`).

**2. Put it in a local `.env` file (recommended):** copy `.env.example` to `.env` in the project root and add the key:

```
GROQ_API_KEY=gsk_your_key_here
```

The `.env` file is ignored by Git; do not commit it. The AI Assistant badge shows **"Direct AI (GPT-4)"** when direct mode is active. This is a display label only: the project still calls Groq and defaults to `openai/gpt-oss-20b`, which is not an actual GPT-4 model. Leave the key empty to stay offline.

**Or use an environment variable:** `$env:GROQ_API_KEY = "gsk_your_key_here"` (or `SCMS_GROQ_API_KEY`).

The application has no in-app key-entry dialog; configure the key in `.env` or an environment variable. Where the key is looked up, highest priority first: process environment variable, legacy `config\groq.conf`, project-root `.env`, then legacy `config\app.conf`.

| Setting | Meaning |
|---------|---------|
| `GROQ_MODE=1` (default) | **Direct AI**: every message goes to Groq, any topic. |
| `GROQ_MODE=0` | Built-in assistant first; Groq only answers what it cannot (questions it does not understand). |
| `GROQ_MODEL` | Default `openai/gpt-oss-20b` (also `openai/gpt-oss-120b`). Groq retires models from time to time (the old `llama-3.3-70b-versatile` and `llama-3.1-8b-instant` were shut down in August 2026). If answers stop working, run `.\build\scms_cli.exe --groq-test` to check available models. |
| `GROQ_SHARE_DETAILS` | `1` lets Groq see the category, status, location and description of the complaint being discussed; `0` never. **Names, phone numbers and user names are never sent.** |

**How it behaves**

* The request runs on a background thread, so the window never freezes: you see *Asking Groq...* and the answer replaces it.
* If Groq is unreachable, rate-limited (429), the key is wrong (401) or the model was retired, you still get the built-in answer **plus a plain-language reason**.
* The answer is shown with the same bullets / numbered steps as built-in answers, in plain ASCII, with a footnote saying it came from Groq and can be wrong.
* The model is told to be a general helpful assistant, never to invent reference numbers / phone numbers / helplines / laws, to point to **112** (fire **101**) when anyone may be in danger, and to ignore any instructions hidden inside complaint text. For a detected emergency the built-in "call 112" line always stays first.
* A reference number the signed-in user may not see is **never** sent to Groq.
* Windows uses the built-in **WinHTTP** (system certificates and proxy settings); nothing extra to install. The key is sent only in the HTTPS `Authorization` header; plain `http://` is refused except for a server on your own computer (used by the tests).

Use it from the terminal without opening the application:

```powershell
$env:GROQ_API_KEY = "gsk_your_key_here"          # optional; .env is loaded automatically
.\build\scms_cli.exe --ask-ai "Explain what a pothole is in one sentence"
.\build\scms_cli.exe --groq-test               # checks key, address and model
```

> Treat the key like a password: do not share `.env`, `config\app.conf` or `config\groq.conf` with a key in it. `.env` and `config\groq.conf` are ignored by Git. Free-tier limits apply (you may see *rate limit reached*).

### Privacy and access

The assistant only reads complaints the signed-in user is allowed to see. Asking about somebody else's reference answers "could not find". The "similar complaints" figure is just a count.

### Code map

| File | Role |
|------|------|
| `backend/AiKnowledge.*` | the playbooks (edit these to teach it new problems or change advice) |
| `backend/AiAgent.*`, `AiInsight.cpp`, `AiChat.cpp` | text reading, SLA clock, priority, advice, triage, chat |
| `backend/AiLlm.cpp`, `GroqClient.*`, `HttpClient.*`, `MiniJson.*` | optional Groq connection: prompt building (privacy rules), HTTPS (WinHTTP), JSON, settings |
| `frontend/opengl/App/ScreensAI.cpp` | the assistant screen, AI card and writing helper |
| `tests/test_ai.cpp`, `tests/test_llm.cpp` | unit tests |

To add a playbook, copy one block in `AiKnowledge.cpp`, change the `id`, `category`, keywords and advice, rebuild, and run `make test-ai`. A leading `!` on a keyword (for example `"!transformer"`) marks a distinctive word that counts double; a trailing `*` matches longer word forms.

---

## Command line (backend) tools

`build\scms_cli.exe` uses the same backend as the application.

```powershell
.\build\scms_cli.exe --check                 # database + assembly self-test + totals
.\build\scms_cli.exe --seed-demo 300         # create 300 demo complaints through the real service layer
.\build\scms_cli.exe --process-queue         # run the FIFO assignment processor until the queue is empty
.\build\scms_cli.exe --demo-status           # how many demo / real complaints exist
.\build\scms_cli.exe --clear-demo            # delete all demo complaints (real data kept); add --users for the demo accounts too

# AI assistant (uses the seeded demo passwords)
.\build\scms_cli.exe --ai-triage water.officer
.\build\scms_cli.exe --ai-advise water.officer SCMS-2026-000126
.\build\scms_cli.exe --ai-ask water.officer "What should I handle first?"
.\build\scms_cli.exe --ai-ask demo.meera.joshi "Which of my complaints needs attention?"
.\build\scms_cli.exe --ask-ai "Explain a pothole in one sentence"   # direct AI from the terminal (needs a Groq key)
.\build\scms_cli.exe --groq-test "How do I report a pothole?"       # check the optional Groq connection
```

---

## Tests

```powershell
make test-ai      # built-in assistant
make test-llm     # optional Groq connection (JSON, requests/responses, settings file, privacy rules)
```

`make test-llm` runs about 245 checks and needs no network. `make test-ai` runs about 700 checks on the AI assistant (keyword matching rules, classification of 46 sample complaints, danger cues, duration parsing, SLA clock, priority rules, intent detection, advice for both audiences, and the integrity of every playbook). No database is needed.

---

## Project layout

```
SmartCityComplaintSystem/
├── Makefile
├── README.md
├── config/app.conf            database + font + map settings
├── .env.example               template for the private API key file
├── assembly/                  NASM: severity score, string routines
├── backend/                   services, models, database access, AI assistant, scms_cli
├── dsa/                       hand-written Stack, Queue, linked list, hash table, sorting, searching
├── database/                  schema.sql, seed*.sql, demo_data.sql, setup_db.sh, db.ps1 (VS Code helper); pgdata/ is created by setup
├── frontend/opengl/           the OpenGL application (App/, UI/, Map/, Charts/, data/)
├── tests/                     test_ai.cpp
├── third_party/stb/           stb_truetype
└── build/                     compiler output (created by make)
```

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Groq says *rejected the API key (401)* | Check that `GROQ_API_KEY` in `.env` is a Groq key (starts with `gsk_`), then run `.\build\scms_cli.exe --groq-test`. |
| Groq says *retired the model* / *model not found* | Run `.\build\scms_cli.exe --groq-test` to see available models, then update `GROQ_MODEL` in `.env`. |
| Groq says *could not connect* / *TLS failed* | Check the internet connection, firewall/antivirus HTTPS scanning and the PC date and time. |
| `make` is not recognised | Run the `$env:Path = ...` line from *Quick start* in this window, or open the MSYS2 UCRT64 shell. |
| `database files are incompatible with server` / *"was initialized by PostgreSQL 17"* | `database\pgdata` was created by an older PostgreSQL. Run `.\database\db.ps1 setup` (or `bash database/setup_db.sh`): it moves the old folder to `database\pgdata_pg17_backup` and creates a PostgreSQL 18 cluster. |
| `.\database\db.ps1` says *scripts are disabled* | `Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass`, then run it again. |
| `PostgreSQL is not reachable` on the login screen | Start the server: `pg_ctl -D database/pgdata -o "-p 5433" -l database/postgres.log -w start`, then press **Retry**. Check `database/postgres.log`. |
| `port 5433 already in use` / `another server might be running` | Another copy is running. Check with `pg_ctl -D database/pgdata status`, stop it, or choose another port (`$env:SCMS_DB_PORT` and the `-o "-p ..."` option). |
| `FATAL: password authentication failed` | The password in `config\app.conf` does not match the role. Use the values in the table above, or `ALTER ROLE scms_app PASSWORD 'scms_app_pw';` as `postgres`. |
| `libpq.dll` / `libfreeglut.dll` not found when starting the `.exe` | The MSYS2 `ucrt64\bin` folder must be on `PATH` (the `$env:Path` line), or copy those DLLs next to the `.exe`. |
| Text is missing or looks wrong | The fonts named in `config\app.conf` (`C:/Windows/Fonts/segoeui.ttf` ...) must exist, or point them to another TrueType font. |
| `running scripts is disabled on this system` | Only affects `.ps1` files. Run `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`, or paste the commands directly. |
| `make all` complains about `tests/test_main.cpp` | Use the Makefile of this version (it skips the missing file), or run `make backend` and `make frontend`. |
| An old `.exe` runs after you changed the code | Run `make frontend` (or `make backend`) again; `make clean` forces a full rebuild. |
