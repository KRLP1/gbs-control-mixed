/** STRUCTS */
interface Struct {
  name: string;
  type: "byte" | "string";
  size: number;
}

/** i18n: UI languages (German default, matches the device default) */
const I18N = {
  de: {
    // Presets section
    legend_resolution: "Auflösung",
    help_res_1: "Wählt eine Ausgabeauflösung aus diesen Voreinstellungen.",
    help_res_2: "Die Auswahl wird zusätzlich für den Start verwendet. Für NTSC-Quellen wird 1280x960 empfohlen, für PAL 1280x1024.",
    help_res_3: 'Über die Option "Passende Presets" in den Einstellungen kann automatisch zwischen beiden umgeschaltet werden.',
    help_res_4: "Die Auswahl einer Auflösung macht sie außerdem zum neuen Start-Preset.",
    btn_passthrough: "Durchleitung",
    help_presets_1: "Um eigene Anpassungen zu speichern: zuerst einen Slot für das neue Preset auswählen, dann in diesem Slot speichern oder daraus laden.",
    help_presets_2: "Die Auswahl eines Slots macht ihn außerdem zum neuen Start-Preset.",
    btn_preset_load: "Preset laden",
    btn_preset_save: "Preset speichern",
    btn_preset_delete: "Preset löschen",
    // Control section
    legend_adc_gain: "ADC-Verstärkung (Helligkeit)",
    help_adc_1: "Gain +/- passt die Verstärkung für das aktuell geladene Preset an.",
    help_adc_2: "Auto-Gain erhöht die Verstärkung, sodass helle Bereiche als Weiß dargestellt werden, und verringert sie wieder, sobald ein Clipping erkannt wird. Zum Kalibrieren einige Sekunden auf ein weißes Bild richten.",
    btn_gain: "Verstärkung",
    legend_image_control: "Bildsteuerung",
    btn_move: "Verschieben",
    btn_scale: "Skalieren",
    btn_borders: "Rahmen",
    // Filters section
    btn_intensity: "Intensität",
    btn_linefilter: "Linienfilter",
    help_filters_1: "Scanlines funktionieren nur bei 240p-Quellen oder bei 480i mit Bob-Deinterlacing.",
    help_filters_2: "Der Linienfilter beseitigt blockartige Pixelartefakte beim Hochskalieren über 480p hinaus und wird empfohlen.",
    btn_peaking: "Kantenanhebung",
    btn_peaking_level: "Peaking-Stufe",
    btn_step: "Sprungantwort",
    help_peaking_1: "Kantenanhebung erhöht den Kontrast an horizontalen Helligkeitssprüngen und wird empfohlen.",
    help_peaking_2: "Peaking-Stufe wählt die Intensität der Kantenanhebung: Aus, Normal, Stark oder Max.",
    help_peaking_3: "Sprungantwort erhöht die Schärfe an horizontalen Farbsprüngen und wird empfohlen.",
    legend_color: "Farbkorrektur",
    btn_brightness: "Helligkeit",
    btn_contrast: "Kontrast",
    btn_default: "Standard",
    help_color_1: "Pb/U gain - ändert die Blau-Verstärkung.",
    help_color_2: "Pr/V gain - ändert die Rot-Verstärkung.",
    // Preferences section
    legend_settings: "Einstellungen",
    pref_matched: "Passende Presets",
    help_matched: "Wenn aktiviert, wird standardmäßig 1280x960 für NTSC 60 und 1280x1024 für PAL 50 verwendet (gilt nicht für 720p-/1080p-Presets).",
    pref_fullheight: "Volle Höhe",
    help_fullheight_1: "Manche Presets nutzen standardmäßig nicht die gesamte vertikale Ausgabeauflösung, wodurch einige Zeilen schwarz bleiben.",
    help_fullheight_2: "Mit aktivierter Full-Height-Option skalieren diese Presets stattdessen so, dass mehr Bildschirmhöhe ausgefüllt wird.",
    help_fullheight_3: "(Betrifft derzeit nur 1920 x 1080)",
    pref_reverse_encoder: "OLED-Menü-Drehregler umkehren",
    help_reverse_1: "Manche Geräte (z. B. ODV-GBS-C) benötigen diese Option aktiviert, damit eine Drehung im Uhrzeigersinn im OLED-Menü nach unten bewegt.",
    help_reverse_2: "Die Einstellung wird auf dem Gerät gespeichert und bleibt über Firmware-Updates hinweg erhalten.",
    pref_scaling: "Niedrige Auflösung: Hochskalieren verwenden",
    help_scaling_1: "VGA-Eingang mit niedriger Auflösung: Durchreichen oder Hochskalieren",
    help_scaling_2: "Quellen mit niedriger Auflösung können entweder direkt durchgereicht oder hochskaliert werden.",
    help_scaling_3: "Beim Hochskalieren kann es zu kleineren Rand-/Skalierungsproblemen kommen, dafür ist die Kompatibilität mit Displays besser.",
    help_scaling_4: "Außerdem werden andere Bildwiederholraten als 60 Hz bisher nicht gut unterstützt.",
    help_scaling_5: 'Als "niedrige Auflösung" gilt derzeit alles bis einschließlich 640x480 (525 aktive Zeilen).',
    pref_output_mode: "Ausgabe RGBHV/Component",
    help_output_1: "Der Standard-Ausgabemodus ist RGBHV, geeignet für VGA-Kabel oder HDMI-Konverter.",
    help_output_2: "Es kann auch ein experimenteller YPbPr-Modus gewählt werden. Die Kompatibilität ist noch lückenhaft.",
    pref_pal60: "Ausgabe-Bildrate: PAL 50 Hz auf 60 Hz erzwingen",
    help_pal60_1: 'Wenn der Fernseher keine 50-Hz-Quellen unterstützt (Anzeige "unbekanntes Format", egal welches Preset), kann diese Option helfen.',
    help_pal60_2: "Die Bildrate wird dadurch weniger flüssig. Neustart erforderlich.",
    pref_extclock: "Externen Taktgenerator deaktivieren",
    help_extclock_1: "Standardmäßig ist der externe Taktgenerator aktiviert, wenn er installiert ist.",
    help_extclock_2: 'Bei Problemen mit anderen Optionen, z. B. "PAL 50 Hz auf 60 Hz erzwingen", kann er deaktiviert werden. Neustart erforderlich.',
    pref_adc_cal: "ADC-Kalibrierung",
    help_adc_cal_1: "Gbscontrol kalibriert die ADC-Offsets beim Start.",
    help_adc_cal_2: "Bei Problemen mit Farbverschiebungen diese Funktion versuchsweise deaktivieren.",
    pref_ftl: "FrameTime Lock aktiv",
    help_ftl_1: "Diese Option hält Eingangs- und Ausgangstiming synchron und behebt den gelegentlich auftretenden horizontalen Bildriss.",
    help_ftl_2: "Es stehen zwei Methoden zur Verfügung. Bei schwarzem Bildschirm oder vertikalem Versatz die andere Methode ausprobieren.",
    pref_ftl_method: "FrameTime-Lock-Methode",
    help_ftl_method_1: "VTotal + VSST: passt vertikale Gesamtzeilenzahl und vertikalen Sync-Beginn gemeinsam an, für präzises Locking.",
    help_ftl_method_2: "Nur VTotal: passt nur die vertikale Gesamtzeilenzahl an, einfacherer Ansatz.",
    pref_deint: "Deinterlace-Methode",
    help_deint_1: "Gbscontrol erkennt interlaced Inhalte und schaltet das Deinterlacing automatisch um.",
    help_deint_2: "Bob-Methode: praktisch kein Deinterlacing, kein zusätzlicher Lag, flackert aber, kombinierbar mit Scanlines",
    help_deint_3: "Motion Adaptive: entfernt Flackern, zeigt aber einige Artefakte bei bewegten Details",
    help_deint_4: "Wenn möglich, die Quelle auf progressive Ausgabe einstellen. Andernfalls wird Motion Adaptive empfohlen.",
    pref_devmode: "Entwicklermodus",
    help_devmode: "Aktiviert das Entwicklermenü mit verschiedenen Debugging-Werkzeugen",
    pref_slot_filters: "Filterung pro Slot speichern",
    help_slot_filters_1: "Wenn aktiviert, stellen gespeicherte Slots ihre eigenen Filtereinstellungen wieder her.",
    help_slot_filters_2: "Wenn deaktiviert, behalten gespeicherte Slots die aktuellen Filtereinstellungen bei.",
    // Developer section
    legend_dev: "Entwickler",
    btn_console: "Konsole umschalten",
    btn_mem_left: "MEM Links",
    btn_mem_right: "MEM Rechts",
    btn_hs_left: "HS Links",
    btn_hs_right: "HS Rechts",
    btn_list_options: "Optionen auflisten",
    btn_show_info: "Info anzeigen",
    btn_get_timings: "Video-Timings abrufen",
    btn_freeze: "Erfassung einfrieren",
    btn_adc_filter: "ADC-Filter",
    btn_sdram: "SDRAM umschalten",
    btn_debug_view: "Debug-Ansicht",
    btn_chip_reset: "Chip zurücksetzen",
    btn_htotal_resync: "HTotal resynchronisieren",
    btn_sync_invert: "Sync invertieren",
    // System section
    btn_ota: "OTA aktivieren",
    btn_restart: "Neustart",
    btn_reset_defaults: "Auf Standard zurücksetzen",
    legend_backup: "Backup [nur für dasselbe Gerät]",
    help_backup_1: "Backup / Wiederherstellung der Konfigurationsdateien",
    help_backup_2: "Backup nur für das aktuelle Gerät gültig",
    btn_restore: "Wiederherstellen",
    legend_wifi: "WLAN",
    legend_ssid_select: "SSID auswählen",
    legend_ssid_connect: "Mit SSID verbinden",
    ph_password: "Passwort",
    btn_connect: "Verbinden",
    help_hostname_1: "Der über mDNS bekanntgegebene Hostname. Zugriff auf das Gerät unter <hostname>.local",
    help_hostname_2: "Erlaubte Zeichen: Buchstaben, Ziffern, Bindestriche. Max. 32 Zeichen. Neustart erforderlich.",
    btn_save_restart: "Speichern & Neustart",
    // Prompt / alert / output
    prompt_title: "Eingabe",
    btn_cancel: "ABBRECHEN",
    alert_title: "HINWEIS",
    legend_output: "Ausgabe",
    // dynamic strings
    prompt_slot_name: "Slot-Namen vergeben",
    slot_empty: "Leer",
    slot_meta_downscale: "VERKLEINERT",
    slot_meta_custom: "EIGEN",
    alert_invalid_backup: "Ungültige Backup-Datei",
    alert_restore_reboot: "GBSControl wird neu gestartet.\nBitte warten, bis WLAN wieder verbunden ist, dann OK klicken",
    wifi_sta_scanning: "STA | Netzwerk suchen",
    alert_invalid_hostname: "Ungültiger Hostname. Nur Buchstaben, Ziffern und Bindestriche erlaubt (kein Bindestrich am Anfang/Ende, max. 32 Zeichen).",
    alert_hostname_set: "Hostname auf '{h}' gesetzt. GBSControl startet neu.\nNach dem Neustart erreichbar unter http://{h}.local/",
    alert_wifi_reboot: "GBSControl startet neu und verbindet sich mit {ssid}. Bitte einige Sekunden warten und dann OK drücken",
    wifi_scanning: "Suche läuft...",
    wifi_no_networks: "Keine Netzwerke gefunden",
    alert_ap_mode: 'Wechsle in den AP-Modus. Bitte mit der SSID "gbscontrol" verbinden und dann OK klicken',
    peak_off: "Peaking: Aus",
    peak_normal: "Peaking: Normal",
    peak_strong: "Peaking: Stark",
    peak_max: "Peaking: Max",
    // Online update
    legend_ota: "Online-Update",
    help_ota_1: "Prüft auf GitHub (KRLP1/gbs-control-mixed) nach neuerer Firmware und installiert sie direkt (mit SHA256-Prüfung)",
    help_ota_2: "Während des Updates wird die Verbindung kurz getrennt; das Gerät startet danach automatisch neu",
    ota_installed: "Installiert:",
    ota_available_label: "Verfügbar:",
    btn_ota_check: "Nach Update suchen",
    btn_ota_start: "Jetzt aktualisieren",
    ota_checking: "Prüfe auf Updates …",
    ota_checking_lost: "Prüfe … Verbindung kurzzeitig unterbrochen (normal)",
    ota_updating_lost: "Update läuft … Verbindung kurzzeitig unterbrochen (normal)",
    ota_downloading: "Download läuft …",
    ota_downloading_pct: "Download läuft … {p}%",
    ota_reboot: "Update erfolgreich! Das Gerät startet jetzt neu …",
    ota_boot_pending: "Update wird gestartet: das Gerät startet neu und flasht die Firmware anschließend im Boot-Modus …",
    ota_up_to_date: "Firmware ist aktuell",
    ota_update_available: "Neue Firmware verfügbar",
    ota_confirm: "Bitte erneut drücken, um das Update zu starten …",
    ota_starting: "Update wird gestartet …",
    ota_back: "Gerät ist zurück (Firmware {v}).",
    ota_busy: "Gerät beschäftigt, bitte später erneut versuchen.",
    ota_unreachable: "Gerät nicht erreichbar.",
    ota_no_answer: "Keine Antwort vom Gerät. Bitte Seite neu laden.",
    ota_err_wifi: "WLAN nicht verbunden",
    ota_err_check: "Prüfung fehlgeschlagen",
    ota_err_download: "Download fehlgeschlagen",
    ota_err_flash: "Flashen fehlgeschlagen",
    ota_err_checksum: "Prüfsummenfehler (SHA256)",
    ota_err_space: "Zu wenig freier Speicher",
    ota_err_oled: "OLED-Menü aktiv, bitte zuerst schließen",
    ota_err_unknown: "Unbekannter Fehler",
    lang_button: "Sprache: Deutsch",
  },
  en: {
    // Presets section
    legend_resolution: "Resolution",
    help_res_1: "Select an output resolution from these presets.",
    help_res_2: "The selection is also used as the startup preset. 1280x960 is recommended for NTSC sources, 1280x1024 for PAL.",
    help_res_3: 'The "Matching presets" option in the settings switches between the two automatically.',
    help_res_4: "Selecting a resolution also makes it the new startup preset.",
    btn_passthrough: "Passthrough",
    help_presets_1: "To save your own tweaks: first select a slot for the new preset, then save to or load from that slot.",
    help_presets_2: "Selecting a slot also makes it the new startup preset.",
    btn_preset_load: "Load preset",
    btn_preset_save: "Save preset",
    btn_preset_delete: "Delete preset",
    // Control section
    legend_adc_gain: "ADC gain (brightness)",
    help_adc_1: "Gain +/- adjusts the gain for the currently loaded preset.",
    help_adc_2: "Auto-gain raises the gain so bright areas show as white, and lowers it again as soon as clipping is detected. Point it at a white image for a few seconds to calibrate.",
    btn_gain: "Gain",
    legend_image_control: "Image control",
    btn_move: "Move",
    btn_scale: "Scale",
    btn_borders: "Borders",
    // Filters section
    btn_intensity: "Intensity",
    btn_linefilter: "Line filter",
    help_filters_1: "Scanlines only work with 240p sources, or 480i with bob deinterlacing.",
    help_filters_2: "The line filter removes blocky pixel artifacts when upscaling beyond 480p and is recommended.",
    btn_peaking: "Peaking",
    btn_peaking_level: "Peaking level",
    btn_step: "Step response",
    help_peaking_1: "Peaking increases the contrast at horizontal brightness transitions and is recommended.",
    help_peaking_2: "Peaking level selects the peaking intensity: off, normal, strong or max.",
    help_peaking_3: "Step response increases sharpness at horizontal color transitions and is recommended.",
    legend_color: "Color correction",
    btn_brightness: "Brightness",
    btn_contrast: "Contrast",
    btn_default: "Default",
    help_color_1: "Pb/U gain - changes the blue gain.",
    help_color_2: "Pr/V gain - changes the red gain.",
    // Preferences section
    legend_settings: "Settings",
    pref_matched: "Matching presets",
    help_matched: "When enabled, 1280x960 is used by default for NTSC 60 and 1280x1024 for PAL 50 (does not apply to 720p/1080p presets).",
    pref_fullheight: "Full height",
    help_fullheight_1: "Some presets do not use the full vertical output resolution by default, which leaves some lines black.",
    help_fullheight_2: "With the full-height option enabled, those presets scale to fill more of the screen height instead.",
    help_fullheight_3: "(Currently only affects 1920 x 1080)",
    pref_reverse_encoder: "Reverse OLED menu rotary encoder",
    help_reverse_1: "Some devices (e.g. ODV-GBS-C) need this option enabled so that a clockwise rotation moves down in the OLED menu.",
    help_reverse_2: "The setting is stored on the device and survives firmware updates.",
    pref_scaling: "Low resolution: use upscaling",
    help_scaling_1: "Low-resolution VGA input: pass through or upscale",
    help_scaling_2: "Low-resolution sources can either be passed through directly or upscaled.",
    help_scaling_3: "Upscaling can cause minor border/scaling issues, but display compatibility is better.",
    help_scaling_4: "Also, refresh rates other than 60 Hz are not yet well supported.",
    help_scaling_5: '"Low resolution" currently means everything up to and including 640x480 (525 active lines).',
    pref_output_mode: "Output RGBHV/component",
    help_output_1: "The default output mode is RGBHV, suitable for VGA cables or HDMI converters.",
    help_output_2: "An experimental YPbPr mode is also available. Compatibility is still patchy.",
    pref_pal60: "Output frame rate: force PAL 50 Hz to 60 Hz",
    help_pal60_1: 'If the TV does not support 50 Hz sources (showing "unknown format", regardless of the preset), this option may help.',
    help_pal60_2: "The frame rate becomes less smooth as a result. Restart required.",
    pref_extclock: "Disable external clock generator",
    help_extclock_1: "By default the external clock generator is enabled when one is installed.",
    help_extclock_2: 'If there are problems with other options, e.g. "force PAL 50 Hz to 60 Hz", it can be disabled. Restart required.',
    pref_adc_cal: "ADC calibration",
    help_adc_cal_1: "GBS-Control calibrates the ADC offsets at startup.",
    help_adc_cal_2: "If you experience color shifts, try disabling this.",
    pref_ftl: "FrameTime lock active",
    help_ftl_1: "This option keeps input and output timing in sync and fixes the occasional horizontal screen tear.",
    help_ftl_2: "Two methods are available. If the screen is black or vertically offset, try the other method.",
    pref_ftl_method: "FrameTime lock method",
    help_ftl_method_1: "VTotal + VSST: adjusts the vertical total line count and the vertical sync start together, for precise locking.",
    help_ftl_method_2: "VTotal only: adjusts only the vertical total line count, a simpler approach.",
    pref_deint: "Deinterlace method",
    help_deint_1: "GBS-Control detects interlaced content and switches the deinterlacing automatically.",
    help_deint_2: "Bob method: virtually no deinterlacing, no added lag, but it flickers; can be combined with scanlines",
    help_deint_3: "Motion adaptive: removes the flicker but shows some artifacts on moving details",
    help_deint_4: "If possible, set the source to progressive output. Otherwise motion adaptive is recommended.",
    pref_devmode: "Developer mode",
    help_devmode: "Enables the developer menu with various debugging tools",
    pref_slot_filters: "Save filters per slot",
    help_slot_filters_1: "When enabled, saved slots restore their own filter settings.",
    help_slot_filters_2: "When disabled, saved slots keep the current filter settings.",
    // Developer section
    legend_dev: "Developer",
    btn_console: "Toggle console",
    btn_mem_left: "MEM left",
    btn_mem_right: "MEM right",
    btn_hs_left: "HS left",
    btn_hs_right: "HS right",
    btn_list_options: "List options",
    btn_show_info: "Show info",
    btn_get_timings: "Get video timings",
    btn_freeze: "Freeze capture",
    btn_adc_filter: "ADC filter",
    btn_sdram: "Toggle SDRAM",
    btn_debug_view: "Debug view",
    btn_chip_reset: "Reset chip",
    btn_htotal_resync: "Resync HTotal",
    btn_sync_invert: "Invert sync",
    // System section
    btn_ota: "Enable OTA",
    btn_restart: "Restart",
    btn_reset_defaults: "Reset to defaults",
    legend_backup: "Backup [intended for same device]",
    help_backup_1: "Back up / restore the configuration files",
    help_backup_2: "A backup is only valid for the device it was created on",
    btn_restore: "Restore",
    legend_wifi: "WiFi",
    legend_ssid_select: "Select SSID",
    legend_ssid_connect: "Connect to SSID",
    ph_password: "Password",
    btn_connect: "Connect",
    help_hostname_1: "The hostname announced via mDNS. Access the device at <hostname>.local",
    help_hostname_2: "Allowed characters: letters, digits, hyphens. Max. 32 characters. Restart required.",
    btn_save_restart: "Save & restart",
    // Prompt / alert / output
    prompt_title: "Input",
    btn_cancel: "CANCEL",
    alert_title: "NOTICE",
    legend_output: "Output",
    // dynamic strings
    prompt_slot_name: "Name the slot",
    slot_empty: "Empty",
    slot_meta_downscale: "DOWNSCALED",
    slot_meta_custom: "CUSTOM",
    alert_invalid_backup: "Invalid backup file",
    alert_restore_reboot: "GBS-Control is restarting.\nPlease wait until WiFi is connected again, then click OK",
    wifi_sta_scanning: "STA | Scanning...",
    alert_invalid_hostname: "Invalid hostname. Only letters, digits and hyphens are allowed (no leading/trailing hyphen, max. 32 characters).",
    alert_hostname_set: "Hostname set to '{h}'. GBS-Control is restarting.\nAfter the restart it will be reachable at http://{h}.local/",
    alert_wifi_reboot: "GBS-Control is restarting and will connect to {ssid}. Please wait a few seconds, then press OK",
    wifi_scanning: "Scanning...",
    wifi_no_networks: "No networks found",
    alert_ap_mode: 'Switching to AP mode. Please connect to the SSID "gbscontrol" and then click OK',
    peak_off: "Peaking: Off",
    peak_normal: "Peaking: Normal",
    peak_strong: "Peaking: Strong",
    peak_max: "Peaking: Max",
    // Online update
    legend_ota: "Online update",
    help_ota_1: "Checks GitHub (KRLP1/gbs-control-mixed) for newer firmware and installs it directly (with SHA256 check)",
    help_ota_2: "The connection is briefly interrupted during the update; the device then restarts automatically",
    ota_installed: "Installed:",
    ota_available_label: "Available:",
    btn_ota_check: "Check for update",
    btn_ota_start: "Update now",
    ota_checking: "Checking for updates …",
    ota_checking_lost: "Checking … connection briefly interrupted (normal)",
    ota_updating_lost: "Update in progress … connection briefly interrupted (normal)",
    ota_downloading: "Downloading …",
    ota_downloading_pct: "Downloading … {p}%",
    ota_reboot: "Update successful! The device is restarting now …",
    ota_boot_pending: "Update starting: the device restarts now and flashes the firmware in boot mode afterwards …",
    ota_up_to_date: "Firmware is up to date",
    ota_update_available: "New firmware available",
    ota_confirm: "Press again to start the update …",
    ota_starting: "Starting update …",
    ota_back: "Device is back (firmware {v}).",
    ota_busy: "Device busy, please try again later.",
    ota_unreachable: "Device not reachable.",
    ota_no_answer: "No response from the device. Please reload the page.",
    ota_err_wifi: "WiFi not connected",
    ota_err_check: "Check failed",
    ota_err_download: "Download failed",
    ota_err_flash: "Flashing failed",
    ota_err_checksum: "Checksum error (SHA256)",
    ota_err_space: "Not enough free space",
    ota_err_oled: "OLED menu active, please close it first",
    ota_err_unknown: "Unknown error",
    lang_button: "Language: English",
  },
} as const;

type I18nLang = keyof typeof I18N;
let LANG: I18nLang = "de";

const t = (key: string): string =>
  (I18N[LANG] && (I18N[LANG] as Record<string, string>)[key]) ||
  (I18N.de as Record<string, string>)[key] ||
  key;

const applyI18n = () => {
  document.querySelectorAll<HTMLElement>("[data-i18n]").forEach((el) => {
    const key = el.getAttribute("data-i18n");
    if (key) {
      el.textContent = t(key);
    }
  });
  document.querySelectorAll<HTMLElement>("[data-i18n-ph]").forEach((el) => {
    const key = el.getAttribute("data-i18n-ph");
    if (key) {
      el.setAttribute("placeholder", t(key));
    }
  });
  document.documentElement.setAttribute("lang", LANG);
  const langLabel = document.querySelector("[gbs-language-label]");
  if (langLabel) {
    langLabel.textContent = t("lang_button");
  }
  // slot meta labels ("Leer"/"VERKLEINERT"/...) need a refresh in the new language
  if (GBSControl.structs) {
    updateSlotNames();
  }
  // the update status line is built from a key, so it can follow the language too
  otaRenderMessage();
};

// Switch language: applies immediately client-side and (optionally) persists
// it on the device so the OLED menu follows after the next restart.
const setLang = (lang: I18nLang, persistToDevice: boolean) => {
  LANG = lang;
  GBSStorage.write("lang", lang);
  applyI18n();
  if (persistToDevice) {
    fetch(`/lang/set?l=${lang}&${+new Date()}`).catch(() => {});
  }
};

interface StructDescriptors {
  [key: string]: Struct[];
}

const Structs: StructDescriptors = {
  slots: [
    { name: "name", type: "string", size: 25 },
    { name: "presetID", type: "byte", size: 1 },
    { name: "scanlines", type: "byte", size: 1 },
    { name: "scanlinesStrength", type: "byte", size: 1 },
    { name: "slot", type: "byte", size: 1 },
    { name: "wantVdsLineFilter", type: "byte", size: 1 },
    { name: "wantStepResponse", type: "byte", size: 1 },
    { name: "wantPeaking", type: "byte", size: 1 },
  ],
};

const StructParser = {
  pos: 0,
  parseStructArray(
    buff: ArrayBuffer | Uint8Array,
    structsDescriptors: StructDescriptors,
    struct: string
  ) {
    const currentStruct = structsDescriptors[struct];

    this.pos = 0;
    const u8 = buff instanceof Uint8Array ? buff : new Uint8Array(buff);

    if (currentStruct) {
      const structSize = StructParser.getSize(structsDescriptors, struct);

      return [...Array(u8.byteLength / structSize)].map(() => {
        return currentStruct.reduce((acc, structItem) => {
          acc[structItem.name] = this.getValue(u8, structItem);
          return acc;
        }, {});
      });
    }

    return null;
  },
  getValue(buff: Uint8Array, structItem: { type: "byte" | "string"; size: number }) {
    switch (structItem.type) {
      case "byte":
        return buff[this.pos++];

      case "string":
        const currentPos = this.pos;
        this.pos += structItem.size;

        return [...Array(structItem.size)]
          .map(() => " ")
          .map((_char, index) => {
            if (buff[currentPos + index] > 31) {
              return String.fromCharCode(buff[currentPos + index]);
            }
            return "";
          })
          .join("")
          .trim();
    }
  },
  getSize(structsDescriptors: StructDescriptors, struct: string) {
    const currentStruct = structsDescriptors[struct];
    return currentStruct.reduce((acc, prop) => {
      acc += prop.size;
      return acc;
    }, 0);
  },
};

/* GBSControl Global Object*/
const GBSControl = {
  buttonMapping: {
    1: "button1280x960",
    2: "button1280x1024",
    3: "button1280x720",
    4: "button720x480",
    5: "button1920x1080",
    6: "button15kHzScaleDown",
    7: "button1080p43",
    8: "buttonSourcePassThrough",
    9: "buttonLoadCustomPreset",
    A: "button1024x768",
  },
  controlKeysMobileMode: "move",
  controlKeysMobile: {
    move: {
      type: "loadDoc",
      left: "7",
      up: "*",
      right: "6",
      down: "/",
    },
    scale: {
      type: "loadDoc",
      left: "h",
      up: "4",
      right: "z",
      down: "5",
    },
    borders: {
      type: "loadUser",
      left: "B",
      up: "C",
      right: "A",
      down: "D",
    },
  },
  dataQueued: 0,
  isWsActive: false,
  maxSlots: 72,
  queuedText: "",
  scanSSIDDone: false,
  scanSSIDRetries: 0,
  serverIP: "",
  structs: null,
  timeOutWs: 0,
  ui: {
    backupButton: null,
    backupInput: null,
    customSlotFilters: null,
    developerSwitch: null,
    loader: null,
    outputClear: null,
    presetButtonList: null,
    progressBackup: null,
    progressRestore: null,
    slotButtonList: null,
    slotContainer: null,
    terminal: null,
    toggleList: null,
    toggleSwichList: null,
    webSocketConnectionWarning: null,
    wifiConnect: null,
    wifiConnectButton: null,
    wifiList: null,
    wifiListTable: null,
    wifiPasswordInput: null,
    wifiSSDInput: null,
    wifiApButton: null,
    wifiStaButton: null,
    wifiStaSSID: null,
    alert: null,
    alertOk: null,
    alertContent: null,
    prompt: null,
    promptOk: null,
    promptCancel: null,
    promptContent: null,
    promptInput: null,
    hostnameInput: null,
    hostnameSaveButton: null,
    languageToggleButton: null,
    otaCurrent: null,
    otaLatest: null,
    otaCheckButton: null,
    otaStartButton: null,
    otaMessage: null,
    otaBar: null,
    otaBarFill: null,
  },
  updateTerminalTimer: 0,
  webSocketServerUrl: "",
  wifi: {
    mode: "ap",
    ssid: "",
  },
  ota: {
    active: false,
    confirmPending: false,
    confirmTimer: 0,
    deadline: 0,
    mode: "check",
    msgArg: "",
    msgKey: "",
    timer: 0,
  },
  ws: null,
  wsCheckTimer: 0,
  wsConnectCounter: 0,
  wsNoSuccessConnectingCounter: 0,
  wsTimeout: 0,
};

/** websocket services */
const checkWebSocketServer = () => {
  if (!GBSControl.isWsActive) {
    if (GBSControl.ws) {
      /*
                        0     CONNECTING
                        1     OPEN
                        2     CLOSING
                        3     CLOSED
                        */
      switch (GBSControl.ws.readyState) {
        case 1:
        case 2:
          GBSControl.ws.close();
          break;
        case 3:
          GBSControl.ws = null;
          break;
      }
    }
    if (!GBSControl.ws) {
      createWebSocket();
    }
  }
};

const timeOutWs = () => {
  console.log("timeOutWs");

  if (GBSControl.ws) {
    GBSControl.ws.close();
  }

  GBSControl.isWsActive = false;
  displayWifiWarning(true);
};

const updateFtlMethodRowsVisibility = (ftlOn: boolean) => {
  document.querySelectorAll("[gbs-ftl-method-rows]").forEach((row) => {
    if (ftlOn) {
      row.removeAttribute("hidden");
    } else {
      row.setAttribute("hidden", "");
    }
  });
};

const createWebSocket = () => {
  if (GBSControl.ws && checkReadyState()) {
    return;
  }

  GBSControl.wsNoSuccessConnectingCounter = 0;
  GBSControl.ws = new WebSocket(GBSControl.webSocketServerUrl, ["arduino"]);

  GBSControl.ws.onopen = () => {
    console.log("ws onopen");

    displayWifiWarning(false);

    GBSControl.wsConnectCounter++;
    clearTimeout(GBSControl.wsTimeout);
    GBSControl.wsTimeout = setTimeout(timeOutWs, 6000);
    GBSControl.isWsActive = true;
    GBSControl.wsNoSuccessConnectingCounter = 0;
  };

  GBSControl.ws.onclose = () => {
    console.log("ws.onclose");

    clearTimeout(GBSControl.wsTimeout);
    GBSControl.isWsActive = false;
  };

  GBSControl.ws.onmessage = (message: any) => {
    clearTimeout(GBSControl.wsTimeout);
    GBSControl.wsTimeout = setTimeout(timeOutWs, 2700);
    GBSControl.isWsActive = true;

    const [
      messageDataAt0,
      messageDataAt1,
      messageDataAt2,
      messageDataAt3,
      messageDataAt4,
      messageDataAt5,
      messageDataAt6,
    ] = message.data;

    if (messageDataAt0 != "#") {
      GBSControl.queuedText += message.data;
      GBSControl.dataQueued += message.data.length;

      if (GBSControl.dataQueued >= 70000) {
        GBSControl.ui.terminal.value = "";
        GBSControl.dataQueued = 0;
      }
    } else {
      const presetId = GBSControl.buttonMapping[messageDataAt1];
      const presetEl = document.querySelector(
        `[gbs-element-ref="${presetId}"]`
      );
      const activePresetButton = presetEl
        ? presetEl.getAttribute("gbs-element-ref")
        : "none";

      GBSControl.ui.presetButtonList.forEach(
        toggleButtonActive(activePresetButton)
      );

      const slotId = "slot-" + messageDataAt2;
      const activeSlotButton = document.querySelector(
        `[gbs-element-ref="${slotId}"]`
      );

      if (activeSlotButton) {
        GBSControl.ui.slotButtonList.forEach(toggleButtonActive(slotId));
      }

      if (messageDataAt3 && messageDataAt4 && messageDataAt5) {
        const optionByte0 = messageDataAt3.charCodeAt(0);
        const optionByte1 = messageDataAt4.charCodeAt(0);
        const optionByte2 = messageDataAt5.charCodeAt(0);
        const optionButtonList = [
          ...nodelistToArray<HTMLButtonElement>(GBSControl.ui.toggleList),
          ...nodelistToArray<HTMLButtonElement>(GBSControl.ui.toggleSwichList),
        ];

        const toggleMethod = (
          button: HTMLTableCellElement | HTMLElement,
          mode: boolean
        ) => {
          if (button.tagName === "TD") {
            button.innerText = mode ? "toggle_on" : "toggle_off";
          }
          button = button.tagName !== "TD" ? button : button.parentElement;
          if (mode) {
            button.setAttribute("active", "");
          } else {
            button.removeAttribute("active");
          }
        };

        optionButtonList.forEach((button) => {
          const toggleData =
            button.getAttribute("gbs-toggle") ||
            button.getAttribute("gbs-toggle-switch");

          switch (toggleData) {
            case "adcAutoGain":
              toggleMethod(button, (optionByte0 & 0x01) == 0x01);
              break;
            case "scanlines":
              toggleMethod(button, (optionByte0 & 0x02) == 0x02);
              break;
            case "vdsLineFilter":
              toggleMethod(button, (optionByte0 & 0x04) == 0x04);
              break;
            case "peaking":
              toggleMethod(button, (optionByte0 & 0x08) == 0x08);
              break;
            case "palForce60":
              toggleMethod(button, (optionByte0 & 0x10) == 0x10);
              break;
            case "wantOutputComponent":
              toggleMethod(button, (optionByte0 & 0x20) == 0x20);
              break;
            /** 1 */

            case "matched":
              toggleMethod(button, (optionByte1 & 0x01) == 0x01);
              break;
            case "frameTimeLock":
              toggleMethod(button, (optionByte1 & 0x02) == 0x02);
              break;
            case "motionAdaptive":
              toggleMethod(button, (optionByte1 & 0x04) == 0x04);
              break;
            case "bob":
              toggleMethod(button, (optionByte1 & 0x04) != 0x04);
              break;
            // case "tap6":
            //   toggleMethod(button, (optionByte1 & 0x08) != 0x04);
            //   break;
            case "step":
              toggleMethod(button, (optionByte1 & 0x10) == 0x10);
              break;
            case "fullHeight":
              toggleMethod(button, (optionByte1 & 0x20) == 0x20);
              break;
            /** 2 */
            case "enableCalibrationADC":
              toggleMethod(button, (optionByte2 & 0x01) == 0x01);
              break;
            case "preferScalingRgbhv":
              toggleMethod(button, (optionByte2 & 0x02) == 0x02);
              break;
            case "disableExternalClockGenerator":
              toggleMethod(button, (optionByte2 & 0x04) == 0x04);
              break;
            case "reverseRotaryEncoderForOledMenu":
              toggleMethod(button, (optionByte2 & 0x08) == 0x08);
              break;
            case "ftlMethodVtotalVsst":
              toggleMethod(button, (optionByte2 & 0x10) == 0);
              break;
            case "ftlMethodVtotalOnly":
              toggleMethod(button, (optionByte2 & 0x10) == 0x10);
              break;
          }
        });

        updateFtlMethodRowsVisibility((optionByte1 & 0x02) === 0x02);
      }

      // Peaking level aus Byte 6 parsen (Bits 0-1).
      // Wird nur gesendet, wenn Firmware >= v1.4.2 (altes Format hat nur 6 Bytes).
      if (messageDataAt6) {
        const peakingByte = messageDataAt6.charCodeAt(0);
        const newLevel = peakingByte & 0x03;
        (GBSControl as any).setPeakingLevel?.(newLevel);
      }
    }
  };
};

const checkReadyState = () => {
  if (GBSControl.ws.readyState == 2) {
    GBSControl.wsNoSuccessConnectingCounter++;

    if (GBSControl.wsNoSuccessConnectingCounter >= 7) {
      console.log("ws still closing, force close");
      GBSControl.ws = null;
      GBSControl.wsNoSuccessConnectingCounter = 0;
      /* fall through */
      createWebSocket();
      return false;
    } else {
      return true;
    }
  } else if (GBSControl.ws.readyState == 0) {
    GBSControl.wsNoSuccessConnectingCounter++;

    if (GBSControl.wsNoSuccessConnectingCounter >= 14) {
      console.log("ws still connecting, retry");
      GBSControl.ws.close();
      GBSControl.wsNoSuccessConnectingCounter = 0;
    }
    return true;
  } else {
    return true;
  }
};

const createIntervalChecks = () => {
  GBSControl.wsCheckTimer = setInterval(checkWebSocketServer, 500);
  GBSControl.updateTerminalTimer = setInterval(updateTerminal, 50);
};

/* API services */

// send a single char command over the open websocket ("s<cmd>" / "u<cmd>"), this avoids
// a new tcp connection + http request on the ESP8266 per button press.
// returns false if the websocket isn't usable, the caller then falls back to http.
const sendWsCommand = (type: string, link: string) => {
  if (
    link.length > 0 &&
    GBSControl.isWsActive &&
    GBSControl.ws &&
    GBSControl.ws.readyState === 1
  ) {
    try {
      GBSControl.ws.send(type + link.charAt(0));
      return true;
    } catch (e) {
      return false;
    }
  }
  return false;
};

const loadDoc = (link: string): Promise<any> => {
  if (sendWsCommand("s", link)) {
    return Promise.resolve();
  }
  return fetch(
    `http://${GBSControl.serverIP}/sc?${link}&nocache=${new Date().getTime()}`
  );
};

const loadUser = (link: string): Promise<any> => {
  if (link == "a" || link == "1") {
    GBSControl.isWsActive = false;
    GBSControl.ui.terminal.value += "\nRestart\n";
    GBSControl.ui.terminal.scrollTop = GBSControl.ui.terminal.scrollHeight;
  }

  if (sendWsCommand("u", link)) {
    return Promise.resolve();
  }
  return fetch(
    `http://${GBSControl.serverIP}/uc?${link}&nocache=${new Date().getTime()}`
  );
};

/** SLOT management */

const savePreset = () => {
  const currentSlot = document.querySelector('[gbs-role="slot"][active]');

  if (!currentSlot) {
    return;
  }

  const key = currentSlot.getAttribute("gbs-element-ref");
  const currentIndex = currentSlot.getAttribute("gbs-slot-id");
  gbsPrompt(
    t("prompt_slot_name"),
    GBSControl.structs.slots[currentIndex].name || key
  )
    .then((currentName: string) => {
      if (currentName && currentName.trim() !== "Leer" && currentName.trim() !== "Empty") {
        currentSlot.setAttribute("gbs-name", currentName);
        currentSlot.removeAttribute("gbs-empty");
        fetch(
          `/slot/save?index=${currentIndex}&name=${encodeURIComponent(
            currentName.substring(0, 24)
          )}&${+new Date()}`
        ).then(() => {
          loadUser("4").then(() => {
            setTimeout(() => {
              fetchSlotNames().then((success: boolean) => {
                if (success) {
                  updateSlotNames();
                }
              });
            }, 500);
          });
        });
      }
    })
    .catch(() => {});
};

const removePreset = () => {
    fetch(`/slot/remove?0&nocache=${new Date().getTime()}`).then(() => {
        setTimeout(() => {
            clearTimeout(GBSControl.wsTimeout);
            // GBSControl.wsTimeout = setTimeout(timeOutWs, 6000); //TODO: calc timeout
            fetch(`/slot/remove?1&nocache=${new Date().getTime()}`).then(() => {
                setTimeout(() => {
                    fetchSlotNames().then((success) => {
                        if (success) {
                            updateSlotNames();
                        }
                    });
                }, 500);
            });
        }, 200);
    });
};

const loadPreset = () => {
  loadUser("3").then(() => {
    if (GBSStorage.read("customSlotFilters") === true) {
      setTimeout(() => {
        fetch(`/gbs/restore-filters?${+new Date()}`);
      }, 250);
    }
  });
};

const getSlotsHTML = () => {
  // prettier-ignore
  return [
    'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
    '0','1','2','3','4','5','6','7','8','9','-','.','_','~','(',')','!','*',':',','
  ].map((chr,idx)=>{

    return `<button
    class="gbs-button gbs-button__slot"
    gbs-slot-id="${idx}"
    gbs-message="${chr}"
    gbs-message-type="setSlot"
    gbs-click="normal"
    gbs-element-ref="slot-${chr}"
    gbs-meta="1024&#xa;x768"
    gbs-role="slot"
    gbs-name="slot-${idx}"
  ></button>`;

  }).join('');
};

const setSlot = (slot: string) => {
  fetch(`/slot/set?slot=${slot}&${+new Date()}`);
};

const updateSlotNames = () => {
  for (let i = 0; i < GBSControl.maxSlots; i++) {
    const el = document.querySelector(`[gbs-slot-id="${i}"]`);

    // Empty slots come back as "Leer" from the device (EMPTY_SLOT_NAME). The label
    // is translated for display; the gbs-empty attribute (not the translated text)
    // lets the stylesheet show only the first empty slot.
    const slotName = GBSControl.structs.slots[i].name;
    const isEmpty = !slotName || slotName === "Leer" || slotName === "Empty";
    el.setAttribute("gbs-name", isEmpty ? t("slot_empty") : slotName);
    if (isEmpty) {
      el.setAttribute("gbs-empty", "");
    } else {
      el.removeAttribute("gbs-empty");
    }
    el.setAttribute(
      "gbs-meta",
      getSlotPresetName(parseInt(GBSControl.structs.slots[i].presetID, 10))
    );
  }
};

const fetchSlotNames = () => {
  return fetch(`/bin/slots.bin?${+new Date()}`)
    .then((response) => response.arrayBuffer())
    .then((arrayBuffer: ArrayBuffer) => {
      if (
        arrayBuffer.byteLength ===
        StructParser.getSize(Structs, "slots") * GBSControl.maxSlots
      ) {
        GBSControl.structs = {
          slots: StructParser.parseStructArray(arrayBuffer, Structs, "slots"),
        };
        return true;
      }
      return false;
    });
};

const getSlotPresetName = (presetID: number) => {
  switch (presetID) {
    case 0x01:
    case 0x011:
      return "1280x960";
    case 0x02:
    case 0x012:
      return "1280x1024";
    case 0x03:
    case 0x013:
      return "1280x720";
    case 0x05:
    case 0x015:
      return "1920x1080";
    case 0x06:
    case 0x016:
      return t("slot_meta_downscale");
    case 0x04:
      return "720x480";
    case 0x14:
      return "768x576";
    case 0x21: // bypass 1
    case 0x22: // bypass 2
      return "BYPASS";
    case 0x07:
      return "1080P 4:3";
    case 0x08:
    case 0x18:
      return "1024x768";
    default:
      return t("slot_meta_custom");
  }
};

const fetchSlotNamesErrorRetry = () => {
  setTimeout(fetchSlotNamesAndInit, 1000);
};

const fetchSlotNamesAndInit = () => {
  fetchSlotNames()
    .then((success) => {
      if (!success) {
        fetchSlotNamesErrorRetry();
        return;
      }
      initUIElements();
      wifiGetStatus().then(() => {
        hostnameGet();
        initUI();
        updateSlotNames();
        createWebSocket();
        createIntervalChecks();
        setTimeout(hideLoading, 1000);
      });
    }, fetchSlotNamesErrorRetry)
    .catch(fetchSlotNamesErrorRetry);
};

/** Promises */
const serial = (funcs: (() => Promise<any>)[]) =>
  funcs.reduce(
    (promise, func) =>
      promise.then((result) =>
        func().then(Array.prototype.concat.bind(result))
      ),
    Promise.resolve([])
  );

/** helpers */

const toggleHelp = () => {
  let help = GBSStorage.read("help") || false;

  GBSStorage.write("help", !help);
  updateHelp(!help);
};

const toggleDeveloperMode = () => {
  const developerMode = GBSStorage.read("developerMode") || false;

  GBSStorage.write("developerMode", !developerMode);
  updateDeveloperMode(!developerMode);
};

const toggleCustomSlotFilters = () => {
  const customSlotFilters = GBSStorage.read("customSlotFilters");
  GBSStorage.write("customSlotFilters", !customSlotFilters);
  updateCustomSlotFilters(!customSlotFilters);
};

const updateHelp = (help: boolean) => {
  if (help) {
    document.body.classList.remove("gbs-help-hide");
  } else {
    document.body.classList.add("gbs-help-hide");
  }
};

const updateDeveloperMode = (developerMode: boolean) => {
  const el = document.querySelector('[gbs-section="developer"]') as HTMLElement;
  if (developerMode) {
    el.removeAttribute("hidden");
    GBSControl.ui.developerSwitch.setAttribute("active", "");
    document.body.classList.remove("gbs-output-hide");
  } else {
    el.setAttribute("hidden", "");
    GBSControl.ui.developerSwitch.removeAttribute("active");
    document.body.classList.add("gbs-output-hide");
  }

  GBSControl.ui.developerSwitch.querySelector(
    ".gbs-icon"
  ).innerText = developerMode ? "toggle_on" : "toggle_off";
};

const updateCustomSlotFilters = (
  customFilters: boolean = GBSStorage.read("customSlotFilters") === true
) => {
  if (customFilters) {
    GBSControl.ui.customSlotFilters.setAttribute("active", "");
  } else {
    GBSControl.ui.customSlotFilters.removeAttribute("active");
  }

  GBSControl.ui.customSlotFilters.querySelector(
    ".gbs-icon"
  ).innerText = customFilters ? "toggle_on" : "toggle_off";
};

const GBSStorage = {
  lsObject: {},
  write(key: string, value: any) {
    GBSStorage.lsObject = GBSStorage.lsObject || {};
    GBSStorage.lsObject[key] = value;
    localStorage.setItem(
      "GBSControlSlotNames",
      JSON.stringify(GBSStorage.lsObject)
    );
  },
  read(key: string): string | number | boolean {
    GBSStorage.lsObject = JSON.parse(
      localStorage.getItem("GBSControlSlotNames") || "{}"
    );
    return GBSStorage.lsObject[key];
  },
};

const nodelistToArray = <Element>(
  nodelist:
    | HTMLCollectionOf<globalThis.Element>
    | NodeListOf<globalThis.Element>
): Element[] => {
  return Array.prototype.slice.call(nodelist);
};

const toggleButtonActive = (id: string) => (
  button: HTMLElement,
  _index: any,
  _array: any
) => {
  button.removeAttribute("active");

  if (button.getAttribute("gbs-element-ref") === id) {
    button.setAttribute("active", "");
  }
};

const displayWifiWarning = (mode: boolean) => {
  // while a device-side online update runs, the websocket is dropped on
  // purpose - do not scare the user with the connection warning
  if (GBSControl.ota.active) {
    mode = false;
  }
  GBSControl.ui.webSocketConnectionWarning.style.display = mode
    ? "block"
    : "none";
};

const updateTerminal = () => {
  if (GBSControl.queuedText.length > 0) {
    requestAnimationFrame(() => {
      let text = GBSControl.ui.terminal.value + GBSControl.queuedText;
      // keep the textarea small, large ones make every append slow (mobile browsers)
      if (text.length > 30000) {
        text = text.substring(text.length - 20000);
      }
      GBSControl.ui.terminal.value = text;
      GBSControl.ui.terminal.scrollTop = GBSControl.ui.terminal.scrollHeight;
      GBSControl.queuedText = "";
    });
  }
};

const updateViewPort = () => {
  document.documentElement.style.setProperty(
    "--viewport-height",
    window.innerHeight + "px"
  );
};

const hideLoading = () => {
  GBSControl.ui.loader.setAttribute("style", "display:none");
};

const checkFetchResponseStatus = (response: Response) => {
  if (!response.ok) {
    throw new Error(`HTTP ${response.status} - ${response.statusText}`);
  }
  return response;
};

const readLocalFile = (file: File) => {
  const reader = new FileReader();
  reader.addEventListener("load", (event) => {
    doRestore(reader.result as ArrayBuffer);
  });
  reader.readAsArrayBuffer(file);
};

/** backup / restore */

const doBackup = () => {
  let backupFiles: string[];
  let done = 0;
  let total = 0;
  fetch("/filesystem/dir")
    .then((r) => r.json())
    .then((files: string[]) => {
      backupFiles = files;
      total = files.length;
      const funcs = files.map((path: string) => () => {
        return fetch(`/filesystem/download?file=${path}&${+new Date()}`).then(
          (response) => {
            GBSControl.ui.progressBackup.setAttribute(
              "gbs-progress",
              `${done}/${total}`
            );
            done++;
            return checkFetchResponseStatus(response) && response.arrayBuffer();
          }
        );
      });

      return serial(funcs);
    })
    .then((files: ArrayBuffer[]) => {
      const headerDescriptor = files.reduce((acc, f, index) => {
        acc[backupFiles[index]] = f.byteLength;
        return acc;
      }, {});

      const backupFilesJSON = JSON.stringify(headerDescriptor);
      const backupFilesJSONSize = backupFilesJSON.length;

      const mainHeader = [
        (backupFilesJSONSize >> 24) & 255, // size
        (backupFilesJSONSize >> 16) & 255, // size
        (backupFilesJSONSize >> 8) & 255, // size
        (backupFilesJSONSize >> 0) & 255,
      ];

      const outputArray: number[] = [
        ...mainHeader,
        ...backupFilesJSON.split("").map((c) => c.charCodeAt(0)),
        ...files.reduce((acc, f, index) => {
          acc = acc.concat(Array.from(new Uint8Array(f)));
          return acc;
        }, []),
      ];

      downloadBlob(
        new Blob([new Uint8Array(outputArray)]),
        `gbs-control.backup-${+new Date()}.bin`
      );
      GBSControl.ui.progressBackup.setAttribute("gbs-progress", ``);
    });
};

const doRestore = (file: ArrayBuffer) => {
  const { backupInput } = GBSControl.ui;
  const fileBuffer = new Uint8Array(file);
  const headerCheck = fileBuffer.slice(4, 6);

  if (headerCheck[0] !== 0x7b || headerCheck[1] !== 0x22) {
    backupInput.setAttribute("disabled", "");
    gbsAlert(t("alert_invalid_backup"))
      .then(
        () => {
          backupInput.removeAttribute("disabled");
        },
        () => {
          backupInput.removeAttribute("disabled");
        }
      )
      .catch(() => {
        backupInput.removeAttribute("disabled");
      });
    return;
  }
  const b0 = fileBuffer[0],
    b1 = fileBuffer[1],
    b2 = fileBuffer[2],
    b3 = fileBuffer[3];
  const headerSize = (b0 << 24) + (b1 << 16) + (b2 << 8) + b3;
  const headerString = Array.from(fileBuffer.slice(4, headerSize + 4))
    .map((c) => String.fromCharCode(c))
    .join("");

  const headerObject = JSON.parse(headerString);
  const files = Object.keys(headerObject);
  let pos = headerSize + 4;
  let total = files.length;
  let done = 0;
  const funcs = files.map((fileName) => () => {
    const fileContents = fileBuffer.slice(pos, pos + headerObject[fileName]);
    const formData = new FormData();
    const uploadName = fileName.startsWith("/") ? fileName.substr(1) : fileName;
    formData.append(
      "file",
      new Blob([fileContents], { type: "application/octet-stream" }),
      uploadName
    );

    return fetch("/filesystem/upload", {
      method: "POST",
      body: formData,
    }).then((response) => {
      GBSControl.ui.progressRestore.setAttribute(
        "gbs-progress",
        `${done}/${total}`
      );
      done++;
      pos += headerObject[fileName];
      return response;
    });
  });

  serial(funcs).then(() => {
    GBSControl.ui.progressRestore.setAttribute("gbs-progress", ``);
    loadUser("a").then(() => {
      gbsAlert(t("alert_restore_reboot"))
        .then(() => {
          window.location.reload();
        })
        .catch(() => {});
    });
  });
};

const downloadBlob = (blob: Blob, name = "file.txt") => {
  // Convert your blob into a Blob URL (a special url that points to an object in the browser's memory)
  const blobUrl = URL.createObjectURL(blob);

  // Create a link element
  const link = document.createElement("a");

  // Set link's href to point to the Blob URL
  link.href = blobUrl;
  link.download = name;

  // Append link to the body
  document.body.appendChild(link);

  // Dispatch click event on the link
  // This is necessary as link.click() does not work on the latest firefox
  link.dispatchEvent(
    new MouseEvent("click", {
      bubbles: true,
      cancelable: true,
      view: window,
    })
  );

  // Remove link from body
  document.body.removeChild(link);
};

/** WIFI management */
const wifiGetStatus = () => {
  return fetch(`/wifi/status?${+new Date()}`)
    .then((r) => r.json())
    .then((wifiStatus: { mode: string; ssid: string }) => {
      GBSControl.wifi = wifiStatus;
      if (GBSControl.wifi.mode === "ap") {
        GBSControl.ui.wifiApButton.setAttribute("active", "");
        GBSControl.ui.wifiApButton.classList.add("gbs-button__secondary");
        GBSControl.ui.wifiStaButton.removeAttribute("active", "");
        GBSControl.ui.wifiStaButton.classList.remove("gbs-button__secondary");
        GBSControl.ui.wifiStaSSID.innerHTML = t("wifi_sta_scanning");
      } else {
        GBSControl.ui.wifiApButton.removeAttribute("active", "");
        GBSControl.ui.wifiApButton.classList.remove("gbs-button__secondary");
        GBSControl.ui.wifiStaButton.setAttribute("active", "");
        GBSControl.ui.wifiStaButton.classList.add("gbs-button__secondary");
        GBSControl.ui.wifiStaSSID.innerHTML = `${GBSControl.wifi.ssid}`;
      }
    });
};

const hostnameGet = () => {
  return fetch(`/hostname/get?${+new Date()}`)
    .then((r) => r.json())
    .then((data: { hostname: string }) => {
      if (data.hostname) {
        GBSControl.ui.hostnameInput.value = data.hostname;
      }
    })
    .catch(() => {});
};

const hostnameSave = () => {
  const hn = GBSControl.ui.hostnameInput.value.trim();
  if (
    !hn.length ||
    hn.length > 32 ||
    !/^[a-zA-Z0-9]([a-zA-Z0-9-]*[a-zA-Z0-9])?$/.test(hn)
  ) {
    gbsAlert(
      t("alert_invalid_hostname")
    );
    return;
  }
  const formData = new FormData();
  formData.append("h", hn);
  fetch("/hostname/set", { method: "POST", body: formData })
    .then((r) => r.json())
    .then((data: { hostname: string }) => {
      gbsAlert(
        t("alert_hostname_set").split("{h}").join(data.hostname)
      )
        .then(() => {
          window.location.href = `http://${data.hostname}.local/`;
        })
        .catch(() => {});
    })
    .catch(() => {});
};

/** firmware online update (runs on the device, triggered and polled from here) */

const OTA_STATE_IDLE = 0;
const OTA_STATE_CHECKING = 1;
const OTA_STATE_UP_TO_DATE = 2;
const OTA_STATE_UPDATE_AVAILABLE = 3;
const OTA_STATE_DOWNLOADING = 4;
const OTA_STATE_REBOOT_PENDING = 5;
const OTA_STATE_ERROR = 6;
// 7 = the ESP8266 accepted the update and restarts FIRST; download and flash
// run in the boot updater afterwards. The UI must NOT report success here.
const OTA_STATE_BOOT_UPDATE_PENDING = 7;

// Error codes of /ota/status ("err"); keep in sync with WebOtaError in WebOta.h.
// The device only sends the code, the text comes from the I18N dictionary.
const otaErrorKey = (err: number): string => {
  switch (err) {
    case 1:
      return "ota_err_wifi";
    case 2:
      return "ota_err_check";
    case 3:
      return "ota_err_download";
    case 4:
      return "ota_err_flash";
    case 5:
      return "ota_err_checksum";
    case 6:
      return "ota_err_space";
    case 7:
      return "ota_err_oled";
    default:
      return "ota_err_unknown";
  }
};

// The status line is stored as dictionary key (+ optional value for a
// placeholder) and rendered from it, so it is translated when the language changes.
const otaRenderMessage = () => {
  const el = GBSControl.ui && GBSControl.ui.otaMessage;
  if (!el) {
    return;
  }
  const key = GBSControl.ota.msgKey;
  el.textContent = key
    ? t(key)
        .split("{p}")
        .join(GBSControl.ota.msgArg)
        .split("{v}")
        .join(GBSControl.ota.msgArg)
    : "";
  // Highlight the result so the feedback doesn't get lost in the gray
  // line: success = blue, error = red, hint = yellow.
  el.classList.remove(
    "gbs-ota__message--ok",
    "gbs-ota__message--error",
    "gbs-ota__message--warn"
  );
  if (
    key === "ota_up_to_date" ||
    key === "ota_update_available" ||
    key === "ota_reboot" ||
    key === "ota_boot_pending" ||
    key === "ota_back"
  ) {
    el.classList.add("gbs-ota__message--ok");
  } else if (
    key.startsWith("ota_err_") ||
    key === "ota_unreachable" ||
    key === "ota_no_answer"
  ) {
    el.classList.add("gbs-ota__message--error");
  } else if (key === "ota_busy" || key === "ota_confirm") {
    el.classList.add("gbs-ota__message--warn");
  }
};

const otaSetMessage = (key: string, arg = "") => {
  GBSControl.ota.msgKey = key;
  GBSControl.ota.msgArg = arg;
  otaRenderMessage();
};

const otaHideWsWarning = () => {
  // the device drops websocket clients while updating - hide the banner early
  GBSControl.ui.webSocketConnectionWarning.style.display = "none";
};

const otaSetButtons = (checkEnabled: boolean, startVisible: boolean) => {
  if (checkEnabled) {
    GBSControl.ui.otaCheckButton.removeAttribute("disabled");
  } else {
    GBSControl.ui.otaCheckButton.setAttribute("disabled", "");
  }
  if (startVisible) {
    GBSControl.ui.otaStartButton.removeAttribute("hidden");
  } else {
    GBSControl.ui.otaStartButton.setAttribute("hidden", "");
  }
};

const otaShowBar = (percent: any) => {
  // percent === null -> indeterminate (no live values, e.g. ESP8266 update)
  GBSControl.ui.otaBar.removeAttribute("hidden");
  if (percent === null) {
    GBSControl.ui.otaBar.classList.add("gbs-ota__bar--indeterminate");
    GBSControl.ui.otaBarFill.style.width = "";
  } else {
    GBSControl.ui.otaBar.classList.remove("gbs-ota__bar--indeterminate");
    GBSControl.ui.otaBarFill.style.width = `${percent}%`;
  }
};

const otaHideBar = () => {
  GBSControl.ui.otaBar.setAttribute("hidden", "");
  GBSControl.ui.otaBar.classList.remove("gbs-ota__bar--indeterminate");
  GBSControl.ui.otaBarFill.style.width = "0%";
};

const otaStopPolling = () => {
  if (GBSControl.ota.timer) {
    clearTimeout(GBSControl.ota.timer);
    GBSControl.ota.timer = 0;
  }
};

const otaSchedulePoll = (delay: number) => {
  otaStopPolling();
  GBSControl.ota.timer = setTimeout(otaPoll, delay);
};

// Poll endpoint. Runs as long as an operation initiated from here is active.
const otaPoll = () => {
  if (!GBSControl.ota.active) {
    return;
  }
  if (Date.now() > GBSControl.ota.deadline) {
    otaStopPolling();
    GBSControl.ota.active = false;
    otaHideBar();
    otaSetButtons(true, false);
    otaSetMessage("ota_no_answer");
    return;
  }

  const controller = new AbortController();
  const abortTimer = setTimeout(() => controller.abort(), 4000);
  fetch(`/ota/status?${+new Date()}`, { signal: controller.signal })
    .then((response) => response.json())
    .then((status: any) => {
      clearTimeout(abortTimer);
      otaHandleStatus(status);
    })
    .catch(() => {
      clearTimeout(abortTimer);
      // Expected while the ESP8266 updater runs: its web server is stopped.
      if (GBSControl.ota.active) {
        otaSetMessage(
          GBSControl.ota.mode === "update"
            ? "ota_updating_lost"
            : "ota_checking_lost"
        );
        if (GBSControl.ota.mode === "update") {
          otaShowBar(null);
        }
        otaSchedulePoll(2500);
      }
    });
};

const otaFinish = (key: string, startVisible = false, arg = "") => {
  otaStopPolling();
  GBSControl.ota.active = false;
  otaHideBar();
  otaSetMessage(key, arg);
  otaSetButtons(true, startVisible);
};

const otaHandleStatus = (status: any) => {
  if (!GBSControl.ota.active) {
    return;
  }
  if (status.current) {
    GBSControl.ui.otaCurrent.textContent = status.current;
  }
  if (status.latest) {
    GBSControl.ui.otaLatest.textContent = status.latest;
  }

  switch (status.state) {
    case OTA_STATE_CHECKING:
      otaSetMessage("ota_checking");
      otaSetButtons(false, false);
      otaSchedulePoll(2000);
      break;
    case OTA_STATE_DOWNLOADING:
      if (status.progress > 0) {
        otaSetMessage("ota_downloading_pct", String(status.progress));
        otaShowBar(status.progress);
      } else {
        otaSetMessage("ota_downloading");
        otaShowBar(null);
      }
      otaSetButtons(false, false);
      otaSchedulePoll(2000);
      break;
    case OTA_STATE_REBOOT_PENDING:
      otaSetMessage("ota_reboot");
      otaHideBar();
      otaSetButtons(false, false);
      // the device reboots on its own; keep polling until it answers again
      GBSControl.ota.deadline = Date.now() + 120000;
      otaSchedulePoll(2500);
      break;
    case OTA_STATE_BOOT_UPDATE_PENDING:
      // ESP8266: nothing is flashed yet - the device restarts and the boot
      // updater does the download/flash (incl. clean retries) while offline.
      // The "update runs" notice shows as soon as the polls start failing.
      otaSetMessage("ota_boot_pending");
      otaShowBar(null);
      otaSetButtons(false, false);
      // clean retries (fresh boot heap per attempt) can stretch the whole
      // boot update past 10 minutes - the poll deadline must cover that
      GBSControl.ota.deadline = Date.now() + 1200000;
      otaSchedulePoll(2500);
      break;
    case OTA_STATE_UP_TO_DATE:
      otaFinish("ota_up_to_date");
      break;
    case OTA_STATE_UPDATE_AVAILABLE:
      otaFinish("ota_update_available", true);
      break;
    case OTA_STATE_ERROR:
      otaFinish(otaErrorKey(status.err));
      break;
    default:
      // IDLE: a fresh boot answered - the device restarted (e.g. after an
      // update) and forgot the OTA state. Show the new version and reload.
      otaFinish("ota_back", false, status.current || "?");
      setTimeout(() => window.location.reload(), 2000);
      break;
  }
};

const otaCheck = () => {
  otaSetMessage("ota_checking");
  otaSetButtons(false, false);
  GBSControl.ui.otaLatest.textContent = "–";
  fetch(`/ota/check?${+new Date()}`)
    .then((response) => response.json())
    .then((data: any) => {
      if (!data.started) {
        otaSetButtons(true, false);
        otaSetMessage("ota_busy");
        return;
      }
      GBSControl.ota.active = true;
      GBSControl.ota.mode = "check";
      GBSControl.ota.deadline = Date.now() + 90000;
      otaHideWsWarning();
      otaSchedulePoll(2500);
    })
    .catch(() => {
      otaSetButtons(true, false);
      otaSetMessage("ota_unreachable");
    });
};

const otaStart = () => {
  // two-step confirm, comparable to ENTER on the OLED update screen
  if (!GBSControl.ota.confirmPending) {
    GBSControl.ota.confirmPending = true;
    otaSetMessage("ota_confirm");
    GBSControl.ota.confirmTimer = window.setTimeout(() => {
      GBSControl.ota.confirmPending = false;
      otaSetMessage("");
    }, 5000);
    return;
  }
  clearTimeout(GBSControl.ota.confirmTimer);
  GBSControl.ota.confirmPending = false;

  otaSetMessage("ota_starting");
  otaSetButtons(false, false);
  otaShowBar(null);
  fetch(`/ota/update?${+new Date()}`)
    .then((response) => response.json())
    .then((data: any) => {
      if (!data.started) {
        otaFinish("ota_busy");
        return;
      }
      GBSControl.ota.active = true;
      GBSControl.ota.mode = "update";
      // the ESP8266 boot update runs offline and may need several clean
      // retries (each a fresh boot + WiFi + partial download); 20 minutes
      // covers the worst case seen so far (5 attempts, 2026-11-08 log)
      GBSControl.ota.deadline = Date.now() + 1200000;
      otaHideWsWarning();
      otaSchedulePoll(2500);
    })
    .catch(() => {
      otaFinish("ota_unreachable");
    });
};

const initOta = () => {
  fetch(`/ota/status?${+new Date()}`)
    .then((response) => response.json())
    .then((status: any) => {
      if (status.current) {
        GBSControl.ui.otaCurrent.textContent = status.current;
      }
      if (status.latest) {
        GBSControl.ui.otaLatest.textContent = status.latest;
      }
      if (status.state === OTA_STATE_UPDATE_AVAILABLE) {
        otaSetMessage("ota_update_available");
        otaSetButtons(true, true);
      } else if (status.state === OTA_STATE_REBOOT_PENDING) {
        // page was (re)loaded while the device is about to restart
        GBSControl.ota.active = true;
        GBSControl.ota.mode = "update";
        GBSControl.ota.deadline = Date.now() + 120000;
        otaHideWsWarning();
        otaSetMessage("ota_reboot");
        otaSchedulePoll(2500);
      } else if (status.state === OTA_STATE_BOOT_UPDATE_PENDING) {
        // page was (re)loaded during the reboot into the boot updater
        GBSControl.ota.active = true;
        GBSControl.ota.mode = "update";
        GBSControl.ota.deadline = Date.now() + 1200000;
        otaHideWsWarning();
        otaSetMessage("ota_boot_pending");
        otaShowBar(null);
        otaSchedulePoll(2500);
      } else if (status.state === OTA_STATE_ERROR) {
        otaSetMessage(otaErrorKey(status.err));
      }
    })
    .catch(() => {
      GBSControl.ui.otaCurrent.textContent = "–";
    });
};

const wifiConnect = () => {
  const ssid = GBSControl.ui.wifiSSDInput.value;
  const password = GBSControl.ui.wifiPasswordInput.value;

  if (!password.length) {
    GBSControl.ui.wifiPasswordInput.classList.add("gbs-wifi__input--error");
    return;
  }

  const formData = new FormData();
  formData.append("n", ssid);
  formData.append("p", password);

  fetch("/wifi/connect", {
    method: "POST",
    body: formData,
  }).then(() => {
    gbsAlert(
      t("alert_wifi_reboot").split("{ssid}").join(ssid)
    )
      .then(() => {
        window.location.href = "http://gbscontrol.local/";
      })
      .catch(() => {});
  });
};

const wifiScanSSID = () => {
  GBSControl.ui.wifiStaButton.setAttribute("disabled", "");
  GBSControl.ui.wifiListTable.innerHTML = "";
  GBSControl.ui.wifiList.removeAttribute("hidden");
  GBSControl.ui.wifiConnect.setAttribute("hidden", "");

  const showWiFiStatusRow = (text: string) => {
    GBSControl.ui.wifiListTable.innerHTML = `<tr><td colspan="3" style="text-align:center;opacity:.8">${text}</td></tr>`;
  };

  if (!GBSControl.scanSSIDDone) {
    GBSControl.scanSSIDRetries = 0;
    showWiFiStatusRow(t("wifi_scanning"));
    fetch(`/wifi/list?${+new Date()}`).then(() => {
      GBSControl.scanSSIDDone = true;
      setTimeout(wifiScanSSID, 3000);
    });
    return;
  }

  fetch(`/wifi/list?${+new Date()}`)
    .then((e) => e.text())
    .then((result) => {
      // Some scans can take longer than 3s; keep polling a few times if empty.
      if (!result.length && GBSControl.scanSSIDRetries < 10) {
        GBSControl.scanSSIDRetries++;
        showWiFiStatusRow(t("wifi_scanning"));
        setTimeout(wifiScanSSID, 1000);
        return null;
      }

      GBSControl.scanSSIDDone = false;
      return result.length
        ? result
            .split("\n")
            .map((line) => line.split(","))
            .map(([strength, encripted, ssid]) => {
              return { strength, encripted, ssid };
            })
        : [];
    })
    .then((ssids) => {
      if (ssids === null) {
        return null;
      }
      return ssids.reduce((acc, ssid) => {
        return `${acc}<tr gbs-ssid="${ssid.ssid}">
        <td class="gbs-icon" style="opacity:${
          parseInt(ssid.strength, 10) / 100
        }">wifi</td>
        <td>${ssid.ssid}</td>
        <td class="gbs-icon">${ssid.encripted ? "lock" : "lock_open"}</td>
      </tr>`;
      }, "");
    })
    .then((html) => {
      if (html === null) {
        return;
      }
      GBSControl.ui.wifiStaButton.removeAttribute("disabled");

      if (html.length) {
        GBSControl.ui.wifiListTable.innerHTML = html;
        GBSControl.ui.wifiList.removeAttribute("hidden");
        GBSControl.ui.wifiConnect.setAttribute("hidden", "");
      } else {
        GBSControl.ui.wifiListTable.innerHTML = `<tr><td colspan="3" style="text-align:center;opacity:.8">${t("wifi_no_networks")}</td></tr>`;
      }
    });
};

const wifiSelectSSID = (event: Event) => {
  (GBSControl.ui
    .wifiSSDInput as HTMLInputElement).value = (event.target as HTMLElement).parentElement.getAttribute(
    "gbs-ssid"
  );
  GBSControl.ui.wifiPasswordInput.classList.remove("gbs-wifi__input--error");
  GBSControl.ui.wifiList.setAttribute("hidden", "");
  GBSControl.ui.wifiConnect.removeAttribute("hidden");
};

const wifiSetAPMode = () => {
  if (GBSControl.wifi.mode === "ap") {
    return;
  }

  const formData = new FormData();
  formData.append("n", "dummy");

  fetch("/wifi/connect", {
    method: "POST",
    body: formData,
  }).then(() => {
    gbsAlert(
      t("alert_ap_mode")
    )
      .then(() => {
        window.location.href = "http://192.168.4.1";
      })
      .catch(() => {});
  });
};

/** button click management */
const controlClick = (control: HTMLButtonElement) => () => {
  const controlKey = control.getAttribute("gbs-control-key");
  const target = GBSControl.controlKeysMobile[GBSControl.controlKeysMobileMode];

  switch (target.type) {
    case "loadDoc":
      loadDoc(target[controlKey]);
      break;
    case "loadUser":
      loadUser(target[controlKey]);
      break;
  }
};

const controlMouseDown = (control: HTMLButtonElement) => () => {
  clearInterval(control["__interval"]);

  const click = controlClick(control);
  click();
  control["__interval"] = setInterval(click, 300);
};

const controlMouseUp = (control: HTMLButtonElement) => () => {
  clearInterval(control["__interval"]);
};

/** inits */
const initMenuButtons = () => {
  const menuButtons = nodelistToArray<HTMLButtonElement>(
    document.querySelector(".gbs-menu").querySelectorAll("button")
  );
  const sections = nodelistToArray<HTMLElement>(
    document.querySelectorAll("section")
  );
  const scroll = document.querySelector(".gbs-scroll");

  menuButtons.forEach((button) =>
    button.addEventListener("click", () => {
      const section = button.getAttribute("gbs-section");

      sections.forEach((section) => section.setAttribute("hidden", ""));
      document
        .querySelector(`section[name="${section}"]`)
        .removeAttribute("hidden");

      menuButtons.forEach((btn) => btn.removeAttribute("active"));
      button.setAttribute("active", "");
      scroll.scrollTo(0, 1);
    })
  );
};

const initGBSButtons = () => {
  const actions = {
    user: loadUser,
    action: loadDoc,
    setSlot,
  };

  const buttons = nodelistToArray<HTMLElement>(
    document.querySelectorAll("[gbs-click]")
  );

  buttons.forEach((button) => {
    const clickMode = button.getAttribute("gbs-click");
    const message = button.getAttribute("gbs-message");
    const messageType = button.getAttribute("gbs-message-type");
    const action = actions[messageType];

    if (clickMode === "normal") {
      button.addEventListener("click", () => {
        action(message);
      });
    }

    if (clickMode === "repeat") {
      const callback = () => {
        action(message);
      };

      button.addEventListener(
        !("ontouchstart" in window) ? "mousedown" : "touchstart",
        () => {
          callback();
          clearInterval(button["__interval"]);
          button["__interval"] = setInterval(callback, 300);
        }
      );
      button.addEventListener(
        !("ontouchstart" in window) ? "mouseup" : "touchend",
        () => {
          clearInterval(button["__interval"]);
        }
      );
    }
  });

  // Peaking level button: device is source of truth (not local counter)
  (() => {
    const peakingBtn = document.querySelector('[gbs-message="8"][gbs-message-type="user"]') as HTMLElement;
    if (!peakingBtn) return;

    const labels = () => [t('peak_off'), t('peak_normal'), t('peak_strong'), t('peak_max')];
    const STORAGE_KEY = 'gbsc_peaking_level';

    // Initial-Guess aus localStorage, Fallback 1 (Normal)
    let stored = parseInt(localStorage.getItem(STORAGE_KEY) ?? '1', 10);
    if (isNaN(stored) || stored < 0 || stored > 3) stored = 1;
    let level = stored;

    const updateLabel = () => {
      const divs = peakingBtn.querySelectorAll('div');
      if (divs.length >= 2) {
        (divs[1] as HTMLElement).textContent = labels()[level];
      }
      peakingBtn.setAttribute('gbs-peaking-level', String(level));
    };
    updateLabel();

    // Optimistisches Inkrement beim Klick; Device-Antwort korrigiert danach.
    peakingBtn.addEventListener('click', () => {
      level = (level + 1) % 4;
      localStorage.setItem(STORAGE_KEY, String(level));
      updateLabel();
    });

    // Expose Setter für onmessage-Handler.
    // Wird mit peakingLevel aus dem Status-Ping (Byte 6, Bits 0-1) aufgerufen.
    (GBSControl as any).setPeakingLevel = (newLevel: number) => {
      if (newLevel >= 0 && newLevel <= 3 && newLevel !== level) {
        level = newLevel;
        localStorage.setItem(STORAGE_KEY, String(level));
        updateLabel();
      }
    };
  })();
};

const initClearButton = () => {
  GBSControl.ui.outputClear.addEventListener("click", () => {
    GBSControl.ui.terminal.value = "";
  });
};

const initControlMobileKeys = () => {
  const controls = document.querySelectorAll("[gbs-control-target]");
  const controlsKeys = document.querySelectorAll("[gbs-control-key]");

  controls.forEach((control) => {
    control.addEventListener("click", () => {
      GBSControl.controlKeysMobileMode = control.getAttribute(
        "gbs-control-target"
      );
      controls.forEach((crtl) => {
        crtl.removeAttribute("active");
      });
      control.setAttribute("active", "");
    });
  });

  controlsKeys.forEach((control) => {
    control.addEventListener(
      !("ontouchstart" in window) ? "mousedown" : "touchstart",
      controlMouseDown(control as HTMLButtonElement)
    );
    control.addEventListener(
      !("ontouchstart" in window) ? "mouseup" : "touchend",
      controlMouseUp(control as HTMLButtonElement)
    );
  });
};

const initLegendHelpers = () => {
  nodelistToArray<HTMLElement>(
    document.querySelectorAll(".gbs-fieldset__legend--help")
  ).forEach((e) => {
    e.addEventListener("click", toggleHelp);
  });
};

const initUnloadListener = () => {
  window.addEventListener("unload", () => {
    clearInterval(GBSControl.wsCheckTimer);
    if (GBSControl.ws) {
      if (GBSControl.ws.readyState == 0 || GBSControl.ws.readyState == 1) {
        GBSControl.ws.close();
      }
    }
  });
};

const initSlotButtons = () => {
  GBSControl.ui.slotContainer.innerHTML = getSlotsHTML();
  GBSControl.ui.slotButtonList = nodelistToArray(
    document.querySelectorAll('[gbs-role="slot"]')
  ) as HTMLElement[];
};

const initUIElements = () => {
  GBSControl.ui = {
    terminal: document.getElementById("outputTextArea"),
    webSocketConnectionWarning: document.getElementById("websocketWarning"),
    presetButtonList: nodelistToArray(
      document.querySelectorAll("[gbs-role='preset']")
    ) as HTMLElement[],
    slotButtonList: nodelistToArray(
      document.querySelectorAll('[gbs-role="slot"]')
    ) as HTMLElement[],
    toggleList: document.querySelectorAll("[gbs-toggle]"),
    toggleSwichList: document.querySelectorAll("[gbs-toggle-switch]"),
    wifiList: document.querySelector("[gbs-wifi-list]"),
    wifiListTable: document.querySelector(".gbs-wifi__list"),
    wifiConnect: document.querySelector(".gsb-wifi__connect"),
    wifiConnectButton: document.querySelector("[gbs-wifi-connect-button]"),
    wifiSSDInput: document.querySelector('[gbs-input="ssid"]'),
    wifiPasswordInput: document.querySelector('[gbs-input="password"]'),
    wifiApButton: document.querySelector("[gbs-wifi-ap]"),
    wifiStaButton: document.querySelector("[gbs-wifi-station]"),
    wifiStaSSID: document.querySelector("[gbs-wifi-station-ssid]"),
    loader: document.querySelector(".gbs-loader"),
    progressBackup: document.querySelector("[gbs-progress-backup]"),
    progressRestore: document.querySelector("[gbs-progress-restore]"),
    outputClear: document.querySelector("[gbs-output-clear]"),
    slotContainer: document.querySelector("[gbs-slot-html]"),
    backupButton: document.querySelector(".gbs-backup-button"),
    backupInput: document.querySelector(".gbs-backup-input"),
    developerSwitch: document.querySelector("[gbs-dev-switch]"),
    customSlotFilters: document.querySelector("[gbs-slot-custom-filters]"),
    alert: document.querySelector('section[name="alert"]'),
    alertOk: document.querySelector("[gbs-alert-ok]"),
    alertContent: document.querySelector("[gbs-alert-content]"),
    prompt: document.querySelector('section[name="prompt"]'),
    promptOk: document.querySelector("[gbs-prompt-ok]"),
    promptCancel: document.querySelector("[gbs-prompt-cancel]"),
    promptContent: document.querySelector("[gbs-prompt-content]"),
    promptInput: document.querySelector('[gbs-input="prompt-input"]'),
    hostnameInput: document.querySelector('[gbs-input="hostname"]'),
    hostnameSaveButton: document.querySelector("[gbs-hostname-save]"),
    languageToggleButton: document.querySelector("[gbs-language-toggle]"),
    otaCurrent: document.querySelector("[gbs-ota-current]"),
    otaLatest: document.querySelector("[gbs-ota-latest]"),
    otaCheckButton: document.querySelector("[gbs-ota-check]"),
    otaStartButton: document.querySelector("[gbs-ota-start]"),
    otaMessage: document.querySelector("[gbs-ota-message]"),
    otaBar: document.querySelector("[gbs-ota-bar]"),
    otaBarFill: document.querySelector("[gbs-ota-bar-fill]"),
  };
};

const initGeneralListeners = () => {
  window.addEventListener("resize", () => {
    updateViewPort();
  });

  GBSControl.ui.backupInput.addEventListener("change", (event) => {
    const fileList: FileList = event.target["files"];
    readLocalFile(fileList[0]);
    GBSControl.ui.backupInput.value = "";
  });

  GBSControl.ui.backupButton.addEventListener("click", doBackup);
  GBSControl.ui.wifiListTable.addEventListener("click", wifiSelectSSID);
  GBSControl.ui.wifiConnectButton.addEventListener("click", wifiConnect);
  GBSControl.ui.wifiApButton.addEventListener("click", wifiSetAPMode);
  GBSControl.ui.wifiStaButton.addEventListener("click", wifiScanSSID);
  GBSControl.ui.hostnameSaveButton.addEventListener("click", hostnameSave);
  GBSControl.ui.otaCheckButton.addEventListener("click", otaCheck);
  GBSControl.ui.otaStartButton.addEventListener("click", otaStart);
  GBSControl.ui.languageToggleButton?.addEventListener("click", () => {
    setLang(LANG === "en" ? "de" : "en", true);
  });
  GBSControl.ui.developerSwitch.addEventListener("click", toggleDeveloperMode);
  GBSControl.ui.customSlotFilters.addEventListener(
    "click",
    toggleCustomSlotFilters
  );

  GBSControl.ui.alertOk.addEventListener("click", () => {
    GBSControl.ui.alert.setAttribute("hidden", "");
    gbsAlertPromise.resolve();
  });

  GBSControl.ui.promptOk.addEventListener("click", () => {
    GBSControl.ui.prompt.setAttribute("hidden", "");
    const value = GBSControl.ui.promptInput.value;
    if (value !== undefined || value.length > 0) {
      gbsPromptPromise.resolve(GBSControl.ui.promptInput.value);
    } else {
      gbsPromptPromise.reject();
    }
  });

  GBSControl.ui.promptCancel.addEventListener("click", () => {
    GBSControl.ui.prompt.setAttribute("hidden", "");
    gbsPromptPromise.reject();
  });

  GBSControl.ui.promptInput.addEventListener("keydown", (event: any) => {
    if (event.keyCode === 13) {
      GBSControl.ui.prompt.setAttribute("hidden", "");
      const value = GBSControl.ui.promptInput.value;
      if (value !== undefined || value.length > 0) {
        gbsPromptPromise.resolve(GBSControl.ui.promptInput.value);
      } else {
        gbsPromptPromise.reject();
      }
    }
    if (event.keyCode === 27) {
      gbsPromptPromise.reject();
    }
  });
};

const initDeveloperMode = () => {
  const devMode = GBSStorage.read("developerMode") as boolean;
  if (devMode === undefined) {
    GBSStorage.write("developerMode", false);
    updateDeveloperMode(false);
  } else {
    updateDeveloperMode(devMode);
  }
};

const initHelp = () => {
  let help = GBSStorage.read("help") as boolean;
  if (help === undefined) {
    help = false;
    GBSStorage.write("help", help);
  }
  updateHelp(help);
};

const gbsAlertPromise = {
  resolve: null,
  reject: null,
};

const alertKeyListener = (event: any) => {
  if (event.keyCode === 13) {
    gbsAlertPromise.resolve();
  }
  if (event.keyCode === 27) {
    gbsAlertPromise.reject();
  }
};

const gbsAlert = (text: string) => {
  GBSControl.ui.alertContent.textContent = text;
  GBSControl.ui.alert.removeAttribute("hidden");
  document.addEventListener("keyup", alertKeyListener);
  return new Promise((resolve, reject) => {
    gbsAlertPromise.resolve = (e) => {
      document.removeEventListener("keyup", alertKeyListener);
      GBSControl.ui.alert.setAttribute("hidden", "");
      return resolve(e);
    };
    gbsAlertPromise.reject = () => {
      document.removeEventListener("keyup", alertKeyListener);
      GBSControl.ui.alert.setAttribute("hidden", "");
      return reject();
    };
  });
};

const gbsPromptPromise = {
  resolve: null,
  reject: null,
};

const gbsPrompt = (text: string, defaultValue = "") => {
  GBSControl.ui.promptContent.textContent = text;
  GBSControl.ui.prompt.removeAttribute("hidden");
  GBSControl.ui.promptInput.value = defaultValue;

  return new Promise<string>((resolve, reject) => {
    gbsPromptPromise.resolve = resolve;
    gbsPromptPromise.reject = reject;
    GBSControl.ui.promptInput.focus();
  });
};

const initUI = () => {
  updateCustomSlotFilters();
  initGeneralListeners();
  updateViewPort();
  initSlotButtons();
  initLegendHelpers();
  initMenuButtons();
  initGBSButtons();
  initClearButton();
  initControlMobileKeys();
  initUnloadListener();
  initDeveloperMode();
  initHelp();
  initOta();
};

const main = () => {
  const ip = location.hostname;
  GBSControl.serverIP = ip;
  GBSControl.webSocketServerUrl = `ws://${ip}:81/`;
  document
    .querySelector(".gbs-loader img")
    .setAttribute(
      "src",
      document.head
        .querySelector('[rel="apple-touch-icon"]')
        .getAttribute("href")
    );

  // UI language: the value stored in the browser is the immediate fallback, then
  // the device is asked as the source of truth (/status/get returns "lang"; the
  // OLED menu follows the same setting after the next restart).
  LANG = GBSStorage.read("lang") === "en" ? "en" : "de";
  applyI18n();
  fetch(`/status/get?${+new Date()}`)
    .then((r) => r.json())
    .then((status: { lang?: string }) => {
      if (status.lang === "en" || status.lang === "de") {
        if (status.lang !== LANG) {
          LANG = status.lang;
          GBSStorage.write("lang", LANG);
          applyI18n();
        }
      }
    })
    .catch(() => {});

  fetchSlotNamesAndInit();
};

main();
