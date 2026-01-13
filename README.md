# Unique Stream Studio — build z działającym interfejsem web

Poniższe kroki opisują kompilację OBS Studio (w tym wbudowanego modułu **obs-browser**, który zapewnia web‑interfejsy w postaci źródeł/przeglądarek i doków) oraz włączenie wbudowanego **obs‑websocket** do zdalnego sterowania z aplikacji webowych.

## Wymagania wstępne

1. **Zainstaluj zależności systemowe** zgodnie z oficjalnym przewodnikiem dla Twojego systemu:
   - https://github.com/obsproject/obs-studio/wiki/Install-Instructions
2. **Wymagane narzędzia build** (minimum):
   - CMake **>= 3.28** (wynika z `CMakePresets.json`).
   - Kompilator C/C++ zgodny z Twoją platformą.
   - Ninja (dla presetów linuksowych).

## Pobranie repozytorium i submodułów

`obs-websocket` jest wymaganym submodułem i musi być obecny przed konfiguracją CMake.

```bash
git clone <URL_DO_REPO>
cd unique-stream-studio
git submodule update --init --recursive
```

## Konfiguracja z włączonym web‑interfejsem (obs-browser + CEF)

Moduł web‑interfejsu w OBS bazuje na Chromium Embedded Framework (**CEF**) i jest kontrolowany opcją CMake **`ENABLE_BROWSER`**. Poniższe komendy używają presetów z `CMakePresets.json` i wymuszają `ENABLE_BROWSER=ON`, aby mieć działające źródła/doki webowe.

### Linux (Ubuntu preset)

```bash
cmake --preset ubuntu -DENABLE_BROWSER=ON
cmake --build --preset ubuntu
```

### macOS

```bash
cmake --preset macos
cmake --build --preset macos
```

> Preset `macos` ma `ENABLE_BROWSER=ON` ustawione domyślnie.

### Windows (x64)

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo
```

> Preset `windows-x64` ma `ENABLE_BROWSER=ON` ustawione domyślnie.

## Uruchomienie i sprawdzenie web‑interfejsu

Po zbudowaniu uruchom OBS (z katalogu build lub po instalacji) i sprawdź:

1. **Źródło „Browser”**:
   - Dodaj nowe źródło → **Browser** → podaj dowolny URL.
   - Jeśli źródło działa, oznacza to poprawnie zbudowany moduł `obs-browser` (CEF).
2. **Dok przeglądarki**:
   - `Widok → Doki → Niestandardowe doki przeglądarki…` i wprowadź URL panelu webowego.
3. **WebSocket do sterowania web‑UI** (opcjonalnie):
   - `Narzędzia → WebSocket Server Settings` → włącz serwer i ustaw port/hasło.
   - Po włączeniu możesz sterować OBS z aplikacji web (np. `obs-websocket-js`).

## Najczęstsze problemy

- **Brak źródła „Browser”** → sprawdź, czy kompilacja została wykonana z `ENABLE_BROWSER=ON` i czy pobrano CEF.
- **Brak obs‑websocket** → upewnij się, że submoduły zostały zainicjalizowane (`git submodule update --init --recursive`).

## Dodatkowe informacje

- Oficjalne instrukcje budowania: https://github.com/obsproject/obs-studio/wiki/Install-Instructions
- Dokumentacja OBS: https://obsproject.com/docs
