# Replay a fake game into a TeamTracker sensor so you can watch the
# TeamTracker Light Show blueprint perform without waiting for kickoff.
#
# This overwrites the sensor's state through Home Assistant's REST API.
# TeamTracker's next poll overwrites it back, so nothing is permanent, but
# point this at a scratch sensor rather than one a real automation watches.
#
# Create a scratch sensor by simply posting to an entity id that does not
# exist yet, for example sensor.gameday_drill, then build an automation
# from the blueprint against it.
#
# Usage:
#   .\fake-game.ps1 -HaUrl "http://homeassistant.local:8123" `
#                   -Token "LONG_LIVED_ACCESS_TOKEN" `
#                   -Sensor "sensor.gameday_drill"

param(
    [Parameter(Mandatory = $true)][string]$HaUrl,
    [Parameter(Mandatory = $true)][string]$Token,
    [Parameter(Mandatory = $true)][string]$Sensor,
    [string]$TeamAbbr = "HOME",
    [string]$OpponentAbbr = "AWAY",
    [string[]]$TeamColors = @("#003594", "#869397"),
    [string[]]$OpponentColors = @("#004C54", "#A5ACAF"),
    [int]$PauseSeconds = 15
)

$headers = @{ Authorization = "Bearer $Token"; "Content-Type" = "application/json" }
$uri = "$HaUrl/api/states/$Sensor"

# POST /api/states replaces all attributes, so every call sends the full set
# the blueprint reads.
function Set-Game([string]$state, [int]$us, [int]$them, $winner = $null) {
    $body = @{
        state      = $state
        attributes = @{
            team_abbr       = $TeamAbbr
            opponent_abbr   = $OpponentAbbr
            team_homeaway   = "home"
            team_colors     = $TeamColors
            opponent_colors = $OpponentColors
            team_score      = $us
            opponent_score  = $them
            team_winner     = $winner
            sport           = "football"
        }
    } | ConvertTo-Json -Depth 4
    Invoke-RestMethod -Method Post -Uri $uri -Headers $headers -Body $body | Out-Null
}

Write-Host "Pre-game"                                    ; Set-Game "PRE" 0 0
Start-Sleep 3
Write-Host "Kickoff (3 team-color pulses)"               ; Set-Game "IN" 0 0
Start-Sleep $PauseSeconds
Write-Host "Field goal, +3 (3 flashes)"                  ; Set-Game "IN" 3 0
Start-Sleep $PauseSeconds
Write-Host "Touchdown, +7 (7 flashes)"                   ; Set-Game "IN" 10 0
Start-Sleep $PauseSeconds
Write-Host "Opponent scores (2 flashes, their colors)"   ; Set-Game "IN" 10 7
Start-Sleep $PauseSeconds
Write-Host "Touchdown, +7 (7 flashes)"                   ; Set-Game "IN" 17 7
Start-Sleep $PauseSeconds
Write-Host "Final 17-7, win (10-flash celebration)"      ; Set-Game "POST" 17 7 $true

Write-Host "Done. TeamTracker's next poll restores the real state."
