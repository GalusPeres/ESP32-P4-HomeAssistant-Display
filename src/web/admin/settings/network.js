
  function toggleStaticIpFields(checkboxId, fieldsId, noteId) {
    const checkbox = document.getElementById(checkboxId);
    const fields = document.getElementById(fieldsId);
    const note = document.getElementById(noteId);
    if (!checkbox || !fields) return;
    const useStatic = checkbox.checked;
    fields.classList.toggle('is-hidden', !useStatic);
    if (note) {
      note.textContent = useStatic
        ? (note.dataset.staticNote || '')
        : (note.dataset.dhcpNote || '');
    }
  }

  function toggleStaticNetworkFields() {
    toggleStaticIpFields(
      'network_use_static', 'network_static_fields',
      'network_ip_mode_note');
  }

  function toggleNetworkSettings() {
    const mode = document.getElementById('network_mode');
    const wifi = document.getElementById('wifi_network_settings');
    if (!mode || !wifi) return;
    const useEthernet = mode.value === 'ethernet';
    wifi.classList.toggle('is-hidden', useEthernet);
  }
