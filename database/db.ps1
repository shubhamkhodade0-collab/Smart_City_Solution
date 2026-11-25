<#
.SYNOPSIS
    Daily database helper for the SmartCity project (VS Code terminal / Windows PowerShell 5.1 or newer).

.DESCRIPTION
    Run it from the project root:

        .\database\db.ps1 status          is the server up? which PostgreSQL version? how many complaints?
        .\database\db.ps1 start           start the project's PostgreSQL server
        .\database\db.ps1 stop            stop it
        .\database\db.ps1 setup           FIRST TIME (or full reset): create cluster + database + tables + accounts
        .\database\db.ps1 connect         open psql as the application user (scms_app)
        .\database\db.ps1 admin           open psql as the superuser (postgres)
        .\database\db.ps1 query "SELECT COUNT(*) FROM complaints;"
        .\database\db.ps1 demo            add 300 demo complaints   (".\database\db.ps1 demo 100" for another amount)
        .\database\db.ps1 demo-status     how many demo / real complaints exist
        .\database\db.ps1 clear-demo      delete ALL demo complaints (real data is kept)   [-Users also removes demo accounts]
        .\database\db.ps1 backup          save the database to exports\scms_backup_<time>.sql
        .\database\db.ps1 version         which PostgreSQL tools this script uses

    It always uses the NEWEST PostgreSQL it finds (PostgreSQL 18.x), looking in $env:SCMS_PG_BIN,
    C:\msys64\ucrt64\bin and C:\Program Files\PostgreSQL\<version>\bin. Settings come from config\app.conf
    (DB_HOST, DB_PORT, DB_NAME, DB_USER, DB_PASSWORD); the environment variables SCMS_DB_PORT ... override them.
#>
param(
    [Parameter(Position = 0)][string]$Action = 'help',
    [Parameter(Position = 1)][string]$Arg1 = '',
    [switch]$Users,     # clear-demo: also remove the demo.* accounts
    [switch]$Force      # clear-demo / setup: do not ask for confirmation
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

# ------------------------------------------------------------------ settings
function Get-Conf([string]$Key, [string]$Default) {
    $env_name = 'SCMS_' + $Key
    $from_env = [Environment]::GetEnvironmentVariable($env_name)
    if ($from_env) { return $from_env.Trim() }
    $file = Join-Path $Root 'config/app.conf'
    if (Test-Path $file) {
        $line = Select-String -Path $file -Pattern ('^' + $Key + '=') | Select-Object -First 1
        if ($line) { return ($line.Line.Substring($Key.Length + 1)).Trim() }
    }
    return $Default
}
$DbHost = Get-Conf 'DB_HOST' 'localhost'
$DbPort = Get-Conf 'DB_PORT' '5433'
$DbName = Get-Conf 'DB_NAME' 'scms'
$DbUser = Get-Conf 'DB_USER' 'scms_app'
$DbPass = Get-Conf 'DB_PASSWORD' 'scms_app_pw'
$SuperPass = 'postgres'
$PgData = Join-Path $Root 'database/pgdata'

# ------------------------------------------------------------------ find the newest PostgreSQL tools
function Find-PgBin {
    $dirs = New-Object System.Collections.ArrayList
    if ($env:SCMS_PG_BIN) { [void]$dirs.Add($env:SCMS_PG_BIN) }
    [void]$dirs.Add('C:\msys64\ucrt64\bin')
    $pf = 'C:\Program Files\PostgreSQL'
    if (Test-Path $pf) {
        Get-ChildItem $pf -Directory -ErrorAction SilentlyContinue | ForEach-Object { [void]$dirs.Add((Join-Path $_.FullName 'bin')) }
    }
    $best = $null
    $bestMajor = -1
    foreach ($d in $dirs) {
        if (-not (Test-Path $d)) { continue }
        $exe = $null
        foreach ($n in @('pg_ctl.exe', 'pg_ctl')) { if (Test-Path (Join-Path $d $n)) { $exe = Join-Path $d $n; break } }
        if (-not $exe) { continue }
        $out = (& $exe --version) -join ' '
        if ($out -match '(\d+)(\.\d+)?') {
            $major = [int]$Matches[1]
            if ($major -gt $bestMajor) { $bestMajor = $major; $best = $d }
        }
    }
    return @{ Dir = $best; Major = $bestMajor }
}
$pg = Find-PgBin
if (-not $pg.Dir) {
    Write-Host 'PostgreSQL tools (pg_ctl, psql) were not found.' -ForegroundColor Red
    Write-Host 'Install PostgreSQL 18 (MSYS2: pacman -S mingw-w64-ucrt-x86_64-postgresql) or set $env:SCMS_PG_BIN to its bin folder.'
    exit 1
}
$sep = [IO.Path]::PathSeparator
if ($env:PATH -notlike ('*' + $pg.Dir + '*')) { $env:PATH = $pg.Dir + $sep + $env:PATH }
$PgMajor = $pg.Major

function Find-Cli {
    foreach ($n in @('scms_cli.exe', 'scms_cli')) {
        $p = Join-Path (Join-Path $Root 'build') $n
        if (Test-Path $p) { return $p }
    }
    return $null
}

function Invoke-Psql([string]$User, [string]$Pass, [string]$Db, [string[]]$PsqlArgs) {
    $env:PGPASSWORD = $Pass
    & psql -h $DbHost -p $DbPort -U $User -d $Db @PsqlArgs
    if ($LASTEXITCODE -ne 0) { throw "psql failed (exit code $LASTEXITCODE)" }
}

function Test-ServerUp {
    & pg_ctl -D $PgData status *> $null
    return ($LASTEXITCODE -eq 0)
}

function Get-ClusterMajor {
    $f = Join-Path $PgData 'PG_VERSION'
    if (Test-Path $f) { return (Get-Content $f -Raw).Trim() }
    return ''
}

function Assert-ClusterVersion {
    $old = Get-ClusterMajor
    if ($old -and ($old -ne [string]$PgMajor)) {
        Write-Host ('database\pgdata was created by PostgreSQL ' + $old + ' but the installed server is PostgreSQL ' + $PgMajor + ', which cannot open it.') -ForegroundColor Yellow
        Write-Host ('Run:  .\database\db.ps1 setup   (moves the old folder aside and creates a PostgreSQL ' + $PgMajor + ' cluster)')
        exit 1
    }
}

function Start-Server {
    Assert-ClusterVersion
    if (-not (Test-Path (Join-Path $PgData 'PG_VERSION'))) { Write-Host 'No database folder yet. Run:  .\database\db.ps1 setup' -ForegroundColor Yellow; exit 1 }
    if (Test-ServerUp) { Write-Host ('PostgreSQL is already running on port ' + $DbPort); return }
    & pg_ctl -D $PgData -o ('-p ' + $DbPort) -l (Join-Path $Root 'database/postgres.log') -w start
    if ($LASTEXITCODE -ne 0) { throw 'the server did not start - see database\postgres.log' }
}

function Stop-Server {
    $old = Get-ClusterMajor
    if ($old -and ($old -ne [string]$PgMajor)) { Write-Host ('Nothing of PostgreSQL ' + $PgMajor + ' is running for this folder.'); return }
    if (-not (Test-ServerUp)) { Write-Host 'The server is not running.'; return }
    & pg_ctl -D $PgData -m fast stop
}

function Require-Server {
    if (-not (Test-ServerUp)) { Write-Host 'The server is not running. Start it first:  .\database\db.ps1 start' -ForegroundColor Yellow; exit 1 }
}

function Require-Cli {
    $cli = Find-Cli
    if (-not $cli) { Write-Host 'build\scms_cli.exe not found. Build it first:  make backend' -ForegroundColor Yellow; exit 1 }
    return $cli
}

# ------------------------------------------------------------------ actions
switch ($Action.ToLower()) {

    'version' {
        Write-Host ('Using PostgreSQL tools from: ' + $pg.Dir)
        & pg_ctl --version
        & psql --version
    }

    'status' {
        Write-Host ('PostgreSQL tools : ' + $pg.Dir + '  (major version ' + $PgMajor + ')')
        $old = Get-ClusterMajor
        if ($old) { Write-Host ('Data folder      : database\pgdata (created by PostgreSQL ' + $old + ')') }
        else { Write-Host 'Data folder      : none yet (run .\database\db.ps1 setup)' }
        if (Test-ServerUp) {
            Write-Host ('Server           : RUNNING on port ' + $DbPort) -ForegroundColor Green
            $cli = Find-Cli
            if ($cli) { & $cli --check } else { Write-Host '(build the backend with "make backend" to see the complaint counts)' }
        } else { Write-Host 'Server           : stopped' -ForegroundColor Yellow }
    }

    'start' { Start-Server }
    'stop' { Stop-Server }
    'restart' { Stop-Server; Start-Server }

    'setup' {
        if ((Test-Path (Join-Path $PgData 'PG_VERSION')) -and -not $Force) {
            Write-Host ('This (re)creates the database "' + $DbName + '": all complaints and accounts in it are replaced by the seed data.') -ForegroundColor Yellow
            $answer = Read-Host 'Type YES to continue'
            if ($answer -ne 'YES') { Write-Host 'Cancelled.'; exit 0 }
        }
        $old = Get-ClusterMajor
        if ($old -and ($old -ne [string]$PgMajor)) {
            if (Test-ServerUp) { throw 'an older server still uses database\pgdata - stop it first' }
            $backup = Join-Path $Root ('database/pgdata_pg' + $old + '_backup')
            if (Test-Path $backup) { $backup = $backup + '_' + (Get-Date -Format 'yyyyMMddHHmmss') }
            Write-Host ('database\pgdata was created by PostgreSQL ' + $old + ', the installed server is PostgreSQL ' + $PgMajor + '.')
            Write-Host ('Moving the old folder to ' + $backup + ' (kept as a backup - delete it when you no longer need it).')
            Move-Item -Path $PgData -Destination $backup
        }
        if (-not (Test-Path (Join-Path $PgData 'PG_VERSION'))) {
            Write-Host ('Initialising a PostgreSQL ' + $PgMajor + ' cluster in database\pgdata')
            $pw = Join-Path $Root 'database/.pwfile'
            Set-Content -Path $pw -Value $SuperPass -Encoding ascii
            & initdb -D $PgData -U postgres -E UTF8 --locale=C --auth=scram-sha-256 --pwfile=$pw | Out-Null
            $code = $LASTEXITCODE
            Remove-Item $pw -Force
            if ($code -ne 0) { throw 'initdb failed' }
        }
        Start-Server
        Write-Host ('Creating role ' + $DbUser + ' and database ' + $DbName)
        Invoke-Psql 'postgres' $SuperPass 'postgres' @('-v', 'ON_ERROR_STOP=1', '-q', '-c', ('DROP DATABASE IF EXISTS ' + $DbName + ';'))
        $env:PGPASSWORD = $SuperPass
        $exists = & psql -h $DbHost -p $DbPort -U postgres -d postgres -Atc ("SELECT 1 FROM pg_roles WHERE rolname='" + $DbUser + "'")
        if (-not $exists) { Invoke-Psql 'postgres' $SuperPass 'postgres' @('-v', 'ON_ERROR_STOP=1', '-q', '-c', ('CREATE ROLE ' + $DbUser + " LOGIN PASSWORD '" + $DbPass + "';")) }
        Invoke-Psql 'postgres' $SuperPass 'postgres' @('-v', 'ON_ERROR_STOP=1', '-q', '-c', ('CREATE DATABASE ' + $DbName + ' OWNER ' + $DbUser + ';'))
        Invoke-Psql 'postgres' $SuperPass $DbName @('-v', 'ON_ERROR_STOP=1', '-q', '-c', 'CREATE EXTENSION IF NOT EXISTS pgcrypto;')
        Invoke-Psql 'postgres' $SuperPass $DbName @('-v', 'ON_ERROR_STOP=1', '-q', '-c', ('ALTER SCHEMA public OWNER TO ' + $DbUser + ';'))
        foreach ($f in @('schema', 'seed_geo', 'seed', 'demo_data')) {
            Write-Host ('Applying ' + $f + '.sql')
            Invoke-Psql $DbUser $DbPass $DbName @('-v', 'ON_ERROR_STOP=1', '-q', '-f', (Join-Path $Root ('database/' + $f + '.sql')))
        }
        Invoke-Psql $DbUser $DbPass $DbName @('-At', '-c', "SELECT 'states='||(SELECT COUNT(*) FROM states)||' cities='||(SELECT COUNT(*) FROM cities)||' users='||(SELECT COUNT(*) FROM users);")
        Write-Host ('Ready: postgresql://' + $DbUser + '@' + $DbHost + ':' + $DbPort + '/' + $DbName) -ForegroundColor Green
        Write-Host 'Add demo complaints with:  .\database\db.ps1 demo'
    }

    'connect' { Require-Server; Invoke-Psql $DbUser $DbPass $DbName @() }
    'admin'   { Require-Server; Invoke-Psql 'postgres' $SuperPass $DbName @() }

    'query' {
        Require-Server
        if (-not $Arg1) { Write-Host 'Usage:  .\database\db.ps1 query "SELECT COUNT(*) FROM complaints;"'; exit 1 }
        Invoke-Psql $DbUser $DbPass $DbName @('-c', $Arg1)
    }

    'demo' {
        Require-Server
        $cli = Require-Cli
        $n = 300
        if ($Arg1) { $n = [int]$Arg1 }
        & $cli --seed-demo $n
        if ($LASTEXITCODE -ne 0) { throw 'seeding failed' }
    }

    'demo-status' { Require-Server; $cli = Require-Cli; & $cli --demo-status }

    'clear-demo' {
        Require-Server
        $cli = Require-Cli
        & $cli --demo-status
        if (-not $Force) {
            $answer = Read-Host 'Delete ALL demo complaints? Real complaints are kept. Type YES to continue'
            if ($answer -ne 'YES') { Write-Host 'Cancelled.'; exit 0 }
        }
        if ($Users) { & $cli --clear-demo --users } else { & $cli --clear-demo }
        if ($LASTEXITCODE -ne 0) { throw 'clearing failed' }
    }

    'backup' {
        Require-Server
        $dir = Join-Path $Root 'exports'
        if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
        $file = Join-Path $dir ('scms_backup_' + (Get-Date -Format 'yyyyMMdd_HHmmss') + '.sql')
        $env:PGPASSWORD = $DbPass
        & pg_dump -h $DbHost -p $DbPort -U $DbUser -d $DbName -f $file
        if ($LASTEXITCODE -ne 0) { throw 'pg_dump failed' }
        Write-Host ('Saved ' + $file)
    }

    default {
        Get-Help $PSCommandPath | Out-String | Write-Host
    }
}
