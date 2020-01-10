[CmdletBinding()]
param ()
$web = New-Object System.Net.WebClient
$web.DownloadFile('https://seed.lokinet.org/lokinet.signed', '%APPDATA%\.lokinet\bootstrap.signed')
