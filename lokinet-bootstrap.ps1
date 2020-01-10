[CmdletBinding()]
param ()
$web = New-Object System.Net.WebClient
if( -not ( Test-Path $env:APPDATA\.lokinet -PathType Container ) )
{
  lokinet.exe -g
}


$web.DownloadFile("https://seed.lokinet.org/lokinet.signed", "$env:APPDATA\.lokinet\bootstrap.signed")
