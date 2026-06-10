# Publish unreal-cli for Windows x64
$ErrorActionPreference = "Stop"

$ProjectDir = Split-Path $PSScriptRoot -Parent
$OutputDir = "$ProjectDir\dist\unreal-cli"

Write-Host "Publishing unreal-cli (win-x64)..."
dotnet publish "$ProjectDir\cli\UnrealCli.Cli\UnrealCli.Cli.csproj" `
    -c Release `
    -r win-x64 `
    --self-contained true `
    -p:PublishSingleFile=true `
    -p:IncludeNativeLibrariesForSelfExtract=true `
    -o "$OutputDir"

Write-Host "Published to: $OutputDir\unreal-cli.exe"
