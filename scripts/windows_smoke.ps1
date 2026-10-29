# Runs on an ephemeral Windows CI account. Exercises real windows and the replacement helper.
$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class SuiteSmoke {
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll")] public static extern bool PostThreadMessage(uint tid, uint msg, IntPtr w, IntPtr l);
}
'@
New-Item 'HKCU:\Software\DesktopSuite' -Force | Out-Null
New-ItemProperty 'HKCU:\Software\DesktopSuite' -Name AutoUpdate -Value 0 -PropertyType DWord -Force | Out-Null
New-ItemProperty 'HKCU:\Software\DesktopSuite' -Name AutoRun -Value 0 -PropertyType DWord -Force | Out-Null
function Wait-Window([string] $class, [string] $title = $null) {
    $deadline = (Get-Date).AddSeconds(30)
    do {
        $h = [SuiteSmoke]::FindWindow($class, $title)
        if ($h -ne [IntPtr]::Zero) { return $h }
        Start-Sleep -Milliseconds 100
    } while ((Get-Date) -lt $deadline)
    throw "Window not found: $class $title"
}
function Quit-Suite([IntPtr] $window) {
    [uint32] $suitePid = 0
    $thread = [SuiteSmoke]::GetWindowThreadProcessId($window, [ref] $suitePid)
    if (-not [SuiteSmoke]::PostThreadMessage($thread, 0x12, [IntPtr]::Zero, [IntPtr]::Zero)) { throw 'Cannot request graceful quit' }
    Wait-Process -Id $suitePid -Timeout 30 -ErrorAction SilentlyContinue
    return $suitePid
}
try {
    Start-Process (Resolve-Path 'build/Release/DesktopSuite.exe')
    $panel = Wait-Window 'DesktopSuite.Edge'
    [SuiteSmoke]::PostMessage($panel, 0x8003, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    [SuiteSmoke]::PostMessage($panel, 0x8002, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    $settings = Wait-Window 'DesktopSuite.Settings'
    # Activate the actual calendar/notes circles through the same mouse hit-test path.
    foreach ($y in @(236,330)) {
        [SuiteSmoke]::PostMessage($panel, 0x8003, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
        Start-Sleep -Milliseconds 1000
        $dpi = [SuiteSmoke]::GetDpiForWindow($panel)
        $xpx = [int](56*$dpi/96)
        $ypx = [int]($y*$dpi/96)
        $position = [IntPtr]($xpx -bor ($ypx -shl 16))
        [SuiteSmoke]::PostMessage($panel, 0x202, [IntPtr]::Zero, $position) | Out-Null
        Start-Sleep -Milliseconds 500
    }
    $calendar = Wait-Window 'DesktopSuite.Tools' 'Calendar & Clock — local time'
    $notes = Wait-Window 'DesktopSuite.Tools' 'Quick Notes — saved automatically'
    $managed = Join-Path $env:LOCALAPPDATA 'DesktopSuite/bin/DesktopSuite.exe'
    if (-not (Test-Path $managed)) { throw 'Per-user installation was not created' }
    $staged = Join-Path $env:LOCALAPPDATA 'DesktopSuite/updates/DesktopSuite-next.exe'
    New-Item (Split-Path $staged) -ItemType Directory -Force | Out-Null
    Copy-Item 'build/Release/DesktopSuite.exe' $staged -Force
    [uint32] $parentId = 0
    [SuiteSmoke]::GetWindowThreadProcessId($panel,[ref] $parentId) | Out-Null
    Start-Process $staged -ArgumentList "--apply-update $parentId"
    Quit-Suite $panel | Out-Null
    $newPanel = Wait-Window 'DesktopSuite.Edge'
    [uint32] $newId=0
    [SuiteSmoke]::GetWindowThreadProcessId($newPanel,[ref] $newId) | Out-Null
    if ($newId -eq $parentId) { throw 'Update did not restart into a new process' }
    if (-not (Test-Path "$managed.bak")) { throw 'Update rollback backup missing' }
    if ((Get-FileHash $managed).Hash -ne (Get-FileHash 'build/Release/DesktopSuite.exe').Hash) { throw 'Updated file hash mismatch' }
    Quit-Suite $newPanel | Out-Null
    python -c "import sqlite3,os; p=os.path.join(os.environ['LOCALAPPDATA'],'DesktopSuite','suite.db'); c=sqlite3.connect(p); e=c.execute('SELECT message,code FROM errors WHERE message LIKE ''%presentation failed%''').fetchall(); assert not e,e; print('Native window creation, per-pixel presentation and updater replacement smoke test passed')"
    if ($LASTEXITCODE -ne 0) { throw 'Rendering error found in SQLite log' }
} finally {
    Get-Process DesktopSuite,DesktopSuite-next -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Remove-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Name DesktopSuite -ErrorAction SilentlyContinue
}
