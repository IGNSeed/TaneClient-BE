param(
    [switch]$Configured
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$vendor = Resolve-Path (Join-Path $root "vendor")
$buildDir = Join-Path $root "build"
$objDir = Join-Path $buildDir "obj"
$distDir = Join-Path $buildDir "dist"
$intermediateDir = Join-Path $buildDir "intermediate"

function Invoke-Tool {
    param(
        [string]$Exe,
        [string[]]$ToolArgs
    )

    & $Exe @ToolArgs
    if ($LASTEXITCODE -ne 0) {
        throw "$Exe failed with exit code $LASTEXITCODE"
    }
}

function Compile-C {
    param(
        [string]$Source,
        [string]$Object
    )

    $includeMinHook = Join-Path $vendor "minhook\include"
    $includeMinHookSrc = Join-Path $vendor "minhook\src"
    $includeHde = Join-Path $vendor "minhook\src\hde"

    Invoke-Tool -Exe "cl.exe" -ToolArgs @(
        "/nologo", "/c", "/O2", "/MT", "/W3",
        "/DWIN32", "/D_WINDOWS", "/D_CRT_SECURE_NO_WARNINGS",
        "/I$includeMinHook", "/I$includeMinHookSrc", "/I$includeHde",
        "/Fo$Object",
        $Source
    )
}

function Compile-Cpp {
    param(
        [string]$Source,
        [string]$Object,
        [string[]]$Includes
    )

    $args = @(
        "/nologo", "/c", "/O2", "/MT", "/EHsc", "/std:c++20", "/W3",
        "/DWIN32", "/D_WINDOWS", "/DWIN32_LEAN_AND_MEAN", "/DNOMINMAX",
        "/DUNICODE", "/D_UNICODE", "/D_CRT_SECURE_NO_WARNINGS"
    )

    foreach ($include in $Includes) {
        $args += "/I$include"
    }

    $args += "/Fo$Object"
    $args += $Source

    Invoke-Tool -Exe "cl.exe" -ToolArgs $args
}

if (-not $Configured) {
    $vcvars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvars)) {
        throw "vcvars64.bat was not found: $vcvars"
    }

    New-Item -ItemType Directory -Force $buildDir | Out-Null
    $script = $PSCommandPath
    & cmd.exe /d /c "cd /d `"$buildDir`" && call `"$vcvars`" && powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$script`" -Configured"
    if ($LASTEXITCODE -ne 0) {
        throw "Configured build failed with exit code $LASTEXITCODE"
    }
    exit 0
}

New-Item -ItemType Directory -Force $objDir, $distDir, $intermediateDir | Out-Null

$minhookSources = @(
    "minhook\src\buffer.c",
    "minhook\src\hook.c",
    "minhook\src\trampoline.c",
    "minhook\src\hde\hde64.c"
)

$minhookObjects = @()
foreach ($relative in $minhookSources) {
    $source = Join-Path $vendor $relative
    $object = Join-Path $objDir (([IO.Path]::GetFileNameWithoutExtension($relative)) + ".obj")
    Compile-C $source $object
    $minhookObjects += $object
}

$imguiDir = Join-Path $vendor "imgui"
$imguiBackends = Join-Path $imguiDir "backends"
$minhookInclude = Join-Path $vendor "minhook\include"
$projectInclude = Join-Path $root "include"
$payloadDir = Join-Path $root "src\payload"
$guiDir = Join-Path $root "src\GUI"
$menuDir = Join-Path $root "src\ImGui"
$movementDir = Join-Path $root "src\Movement"
$renderDir = Join-Path $root "src\Render"
$cameraDir = Join-Path $root "src\Camera"
$patchDir = Join-Path $root "src\Patch"

$payloadSources = @(
    (Join-Path $payloadDir "DllMain.cpp"),
    (Join-Path $payloadDir "D3D11ImGuiHook.cpp"),
    (Join-Path $guiDir "CPS.cpp"),
    (Join-Path $guiDir "FPS.cpp"),
    (Join-Path $guiDir "Ping.cpp"),
    (Join-Path $guiDir "KeyStroke.cpp"),
    (Join-Path $guiDir "ControllerOverlay.cpp"),
    (Join-Path $guiDir "Tab.cpp"),
    (Join-Path $guiDir "ArrowCounter.cpp"),
    (Join-Path $guiDir "PotCounter.cpp"),
    (Join-Path $guiDir "EffectHUD.cpp"),
    (Join-Path $guiDir "ItemHUD.cpp"),
    (Join-Path $menuDir "Menu.cpp"),
    (Join-Path $menuDir "MenuState.cpp"),
    (Join-Path $menuDir "MenuWidgets.cpp"),
    (Join-Path $menuDir "InputBlock.cpp"),
    (Join-Path $renderDir "NameTags.cpp"),
    (Join-Path $renderDir "Fullbright.cpp"),
    (Join-Path $renderDir "NoFog.cpp"),
    (Join-Path $renderDir "Hitbox.cpp"),
    (Join-Path $renderDir "Tracer.cpp"),
    (Join-Path $cameraDir "Zoom.cpp"),
    (Join-Path $cameraDir "FreeLook.cpp"),
    (Join-Path $movementDir "AutoSprint.cpp"),
    (Join-Path $patchDir "ForceCloseOreUI.cpp"),
    (Join-Path $imguiDir "imgui.cpp"),
    (Join-Path $imguiDir "imgui_draw.cpp"),
    (Join-Path $imguiDir "imgui_tables.cpp"),
    (Join-Path $imguiDir "imgui_widgets.cpp"),
    (Join-Path $imguiBackends "imgui_impl_dx11.cpp"),
    (Join-Path $imguiBackends "imgui_impl_dx12.cpp"),
    (Join-Path $imguiBackends "imgui_impl_win32.cpp")
)

$payloadIncludes = @($projectInclude, $payloadDir, $guiDir, $menuDir, $movementDir, $renderDir, $cameraDir, $patchDir, $imguiDir, $imguiBackends, $minhookInclude)
$payloadObjects = @()
foreach ($source in $payloadSources) {
    $object = Join-Path $objDir (([IO.Path]::GetFileNameWithoutExtension($source)) + ".obj")
    Compile-Cpp $source $object $payloadIncludes
    $payloadObjects += $object
}

$payloadDll = Join-Path $intermediateDir "TanePayload.payload"
$payloadImplib = Join-Path $intermediateDir "TanePayload.lib"
$payloadLogoResourceFile = Join-Path $buildDir "PayloadLogoResource.rc"
$payloadLogoResourceObj = Join-Path $objDir "PayloadLogoResource.res"
$payloadLogoForRc = (Join-Path $vendor "images\TaneClient_logo.png").Replace("\", "/")
$payloadFontForRc = (Join-Path $vendor "fonts\JetBrainsMono-VariableFont_wght.ttf").Replace("\", "/")
$payloadBoldFontForRc = (Join-Path $vendor "fonts\JetBrainsMono-Bold.ttf").Replace("\", "/")
$payloadItemHudFontForRc = (Join-Path $vendor "fonts\Minecraftia-Regular.ttf").Replace("\", "/")
$effectIconNames = @(
    "absorption_effect.png",
    "bad_omen_effect.png",
    "blindness_effect.png",
    "breath_of_the_nautilus_effect.png",
    "conduit_power_effect.png",
    "darkness_effect.png",
    "fire_resistance_effect.png",
    "haste_effect.png",
    "health_boost_effect.png",
    "hunger_effect.png",
    "infested_effect.png",
    "invisibility_effect.png",
    "jump_boost_effect.png",
    "levitation_effect.png",
    "mining_fatigue_effect.png",
    "nausea_effect.png",
    "night_vision_effect.png",
    "oozing_effect.png",
    "poison_effect.png",
    "raid_omen_effect.png",
    "regeneration_effect.png",
    "resistance_effect.png",
    "slow_falling_effect.png",
    "slowness_effect.png",
    "speed_effect.png",
    "strength_effect.png",
    "trial_omen_effect.png",
    "village_hero_effect.png",
    "water_breathing_effect.png",
    "weakness_effect.png",
    "weaving_effect.png",
    "wind_charged_effect.png",
    "wither_effect.png"
)
$effectIconResourceLines = @()
for ($i = 0; $i -lt $effectIconNames.Count; $i++) {
    $effectIconPath = (Join-Path $vendor ("images\effect\" + $effectIconNames[$i])).Replace("\", "/")
    $effectIconResourceLines += ("{0} RCDATA ""{1}""" -f (3000 + $i), $effectIconPath)
}
$itemImageNames = @(
    "diamond_helmet.png",
    "diamond_chestplate.png",
    "diamond_leggings.png",
    "diamond_boots.png",
    "diamond_sword.png",
    "arrow.png",
    "potion_bottle_splash_heal.png"
)
$itemImageResourceLines = @()
for ($i = 0; $i -lt $itemImageNames.Count; $i++) {
    $itemImagePath = Join-Path $vendor ("images\items\" + $itemImageNames[$i])
    if (Test-Path -LiteralPath $itemImagePath -PathType Leaf) {
        $itemImagePathForRc = $itemImagePath.Replace("\", "/")
        $itemImageResourceLines += ("{0} RCDATA ""{1}""" -f (4000 + $i), $itemImagePathForRc)
    }
}
Set-Content -Path $payloadLogoResourceFile -Encoding ASCII -Value @"
201 RCDATA "$payloadLogoForRc"
202 RCDATA "$payloadFontForRc"
203 RCDATA "$payloadBoldFontForRc"
204 RCDATA "$payloadItemHudFontForRc"
$($effectIconResourceLines -join "`r`n")
$($itemImageResourceLines -join "`r`n")
"@

Invoke-Tool -Exe "rc.exe" -ToolArgs @(
    "/nologo",
    "/fo", $payloadLogoResourceObj,
    $payloadLogoResourceFile
)

Invoke-Tool -Exe "link.exe" -ToolArgs (
    @("/NOLOGO", "/DLL", "/OUT:$payloadDll", "/IMPLIB:$payloadImplib") +
    $payloadObjects +
    $minhookObjects +
    @($payloadLogoResourceObj, "advapi32.lib", "d3d11.lib", "d3d12.lib", "dxgi.lib", "d3dcompiler.lib", "user32.lib", "gdi32.lib", "dwmapi.lib", "ole32.lib", "windowscodecs.lib")
)

$launcherDir = Join-Path $root "src\launcher"
$resourceFile = Join-Path $buildDir "PayloadResource.rc"
$resourceObj = Join-Path $objDir "PayloadResource.res"
$payloadForRc = $payloadDll.Replace("\", "/")
$logoForRc = (Join-Path $vendor "images\TaneClient_logo.png").Replace("\", "/")
$fontForRc = (Join-Path $vendor "fonts\JetBrainsMono-VariableFont_wght.ttf").Replace("\", "/")
$iconForRc = (Join-Path $vendor "images\TaneClient_logo.ico").Replace("\", "/")
Set-Content -Path $resourceFile -Encoding ASCII -Value @"
104 ICON "$iconForRc"
101 RCDATA "$payloadForRc"
102 RCDATA "$logoForRc"
103 RCDATA "$fontForRc"
"@

Invoke-Tool -Exe "rc.exe" -ToolArgs @(
    "/nologo",
    "/i", $launcherDir,
    "/fo", $resourceObj,
    $resourceFile
)

$launcherObjects = @()
$launcherMainObject = Join-Path $objDir "LauncherMain.obj"
Compile-Cpp (Join-Path $launcherDir "Main.cpp") $launcherMainObject @($launcherDir, $imguiDir, $imguiBackends)
$launcherObjects += $launcherMainObject

$launcherAppObject = Join-Path $objDir "LauncherApp.obj"
Compile-Cpp (Join-Path $launcherDir "LauncherApp.cpp") $launcherAppObject @($launcherDir, $imguiDir, $imguiBackends)
$launcherObjects += $launcherAppObject

$launcherImGuiObjects = @(
    (Join-Path $objDir "imgui.obj"),
    (Join-Path $objDir "imgui_draw.obj"),
    (Join-Path $objDir "imgui_tables.obj"),
    (Join-Path $objDir "imgui_widgets.obj"),
    (Join-Path $objDir "imgui_impl_dx11.obj"),
    (Join-Path $objDir "imgui_impl_win32.obj")
)

$launcherExe = Join-Path $distDir "TaneClient.exe"
Invoke-Tool -Exe "link.exe" -ToolArgs (
    @(
        "/NOLOGO",
        "/SUBSYSTEM:WINDOWS",
        "/OUT:$launcherExe"
    ) +
    $launcherObjects +
    $launcherImGuiObjects +
    @(
        $resourceObj,
        "advapi32.lib",
        "d3d11.lib",
        "dwmapi.lib",
        "dxgi.lib",
        "shell32.lib",
        "user32.lib",
        "gdi32.lib",
        "ole32.lib",
        "windowscodecs.lib"
    )
)

Write-Host "Built:"
Write-Host "  $launcherExe"
