(function(){
  const el = id => document.getElementById(id);
  const hvPoints = [];
  const evPoints = [];
  const ratePoints = [];
  const t0 = Date.now();
  const tx = () => (Date.now() - t0) / 1000.0;
  function appendLog(msg){ const area = el('log'); area.textContent += msg + '\n'; area.scrollTop = area.scrollHeight; }

  // Load and save field values from localStorage
  function saveField(id) {
    const elem = el(id);
    if (elem) localStorage.setItem(id, elem.value);
  }
  function loadField(id, defaultVal) {
    const elem = el(id);
    if (elem) {
      const saved = localStorage.getItem(id);
      if (saved !== null) elem.value = saved;
      else if (defaultVal !== undefined) elem.value = defaultVal;
    }
  }
  function loadSelect(id) {
    const elem = el(id);
    if (elem) {
      const saved = localStorage.getItem(id);
      if (saved !== null) {
        // Wait a bit for options to be loaded
        setTimeout(() => {
          const opt = Array.from(elem.options).find(o => o.value === saved);
          if (opt) elem.value = saved;
        }, 100);
      }
    }
  }

  // Load YAML files on page load
  (async function loadYamlFiles(){
    try {
      const res = await fetch('/yaml/list');
      const data = await res.json();
      const sel = el('m_yaml');
      sel.innerHTML = '';
      if(data.files && data.files.length > 0){
        data.files.forEach(f => {
          const opt = document.createElement('option');
          opt.value = f;
          opt.textContent = f;
          sel.appendChild(opt);
        });
      } else {
        sel.innerHTML = '<option value="">No YAML files found</option>';
      }
      loadSelect('m_yaml');
    } catch(e) {
      el('m_yaml').innerHTML = '<option value="">Error loading files</option>';
    }
  })();

  // Load saved field values
  loadField('hv_device', 'COM10');
  loadField('hv_channel', '1');
  loadField('hv_baud', '9600');
  loadField('hv_value', '1800');
  loadField('hv_interval', '2');
  loadField('m_out', './data_output');
  loadField('m_exe', 'WaveDemo_x743.exe');
  loadField('m_maxev', '0');
  loadField('m_maxt', '30');
  loadField('m_hvseq', '');
  loadField('m_thrseq', '');
  loadField('m_repeat', '1');
  loadField('m_source', '');
  loadField('m_scint', '');

  // Save field values on change
  const fieldIds = ['hv_device', 'hv_channel', 'hv_baud', 'hv_value', 'hv_interval', 'm_yaml', 'm_out', 'm_exe', 'm_maxev', 'm_maxt', 'm_hvseq', 'm_thrseq', 'm_repeat', 'm_source', 'm_scint'];
  fieldIds.forEach(id => {
    const elem = el(id);
    if (elem) elem.addEventListener('change', () => saveField(id));
  });

  // Clean incoming measurement log lines: remove timestamp, drop batch progress lines
  function cleanMeasureLine(line){
    if(!line) return null;
    // Skip raw batch progress lines from WaveDemo
    if(/Batch mode progres+s?/i.test(line)) return null;
    // Strip leading timestamp: YYYY-MM-DD HH:MM:SS,mmm
    return line.replace(/^\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2},\d{3}\s*/, '');
  }

  // Charts
  const hvChart = new Chart(el('chart_hv'), { type:'line', data:{ datasets:[{ label:'HV (V)', data:hvPoints, borderColor:'#2a6', tension:0.1, pointRadius:0 }] }, options:{ responsive:true, animation:false, scales:{ x:{ type:'linear', title:{ display:true, text:'Time (s)'} }, y:{ title:{ display:true, text:'V'} } } } });
  const evChart = new Chart(el('chart_events'), { type:'line', data:{ datasets:[{ label:'Events', data:evPoints, borderColor:'#26c', tension:0.1, pointRadius:0 }] }, options:{ responsive:true, animation:false, scales:{ x:{ type:'linear', title:{ display:true, text:'Time (s)'} }, y:{ beginAtZero:true } } } });
  const rateChart = new Chart(el('chart_rate'), { type:'line', data:{ datasets:[{ label:'Rate (1/s)', data:ratePoints, borderColor:'#c62', tension:0.1, pointRadius:0 }] }, options:{ responsive:true, animation:false, scales:{ x:{ type:'linear', title:{ display:true, text:'Time (s)'} }, y:{ beginAtZero:true } } } });

  // HV websocket
  let wsHV = null;
  let hvMonitoring = false;
  function setHVStatus(connected){
    const badge = el('hv_status');
    if(!badge) return;
    if(connected){ badge.textContent = 'ON'; badge.classList.remove('disconnected'); badge.classList.add('connected'); }
    else { badge.textContent = 'OFF'; badge.classList.remove('connected'); badge.classList.add('disconnected'); }
  }
  function openHV(){
    const dev = encodeURIComponent(el('hv_device').value);
    const ch = encodeURIComponent(el('hv_channel').value);
    const baud = encodeURIComponent(el('hv_baud').value);
    const interval = Number(el('hv_interval').value) || 2;
    wsHV = new WebSocket('ws://' + location.host + '/ws/hv?interval=' + interval + '&device=' + dev + '&channel=' + ch + '&baudrate=' + baud);
    wsHV.onopen = function(){ setHVStatus(true); appendLog('[HV] Monitoring started: MON VMON every ' + interval + 's'); };
    wsHV.onmessage = function(ev){
      try {
        const data = JSON.parse(ev.data);
        if(data.hv !== undefined){
          const y = Number(data.hv);
          if(!Number.isNaN(y)){
            hvPoints.push({ x: tx(), y: y });
            hvChart.update();
            appendLog('[HV] MON VMON -> ' + y);
          }
        }
        if(data.error){ appendLog('[HV] ' + data.error); }
      } catch(e) { /* ignore */ }
    };
    wsHV.onclose = function(){ setHVStatus(false); appendLog('[HV] Monitoring stopped'); if(hvMonitoring){ setTimeout(openHV, 3000); } };
  }
  function stopHV(){ if(wsHV){ try{ wsHV.close(); }catch(e){} wsHV=null; } setHVStatus(false); }
  const toggleBtn = el('btn_hv_toggle');
  if(toggleBtn){
    toggleBtn.onclick = function(){
      if(!hvMonitoring){ hvMonitoring = true; toggleBtn.textContent = 'Stop Monitoring'; openHV(); }
      else { hvMonitoring = false; toggleBtn.textContent = 'Start Monitoring'; stopHV(); }
    };
  }

  // Measurement websocket
  let wsM = null;
  function connectMeasure(id){
    if(wsM){ wsM.close(); wsM = null; }
    wsM = new WebSocket('ws://' + location.host + '/ws/measure/' + id);
    wsM.onmessage = function(ev){
      try {
        const d = JSON.parse(ev.data);
        if(d.last_line){
          const cleaned = cleanMeasureLine(d.last_line);
          if(cleaned) appendLog(cleaned);
        }
        if(d.progress_line){
          appendLog(d.progress_line);
        }
        // Progress bar and labels: show total elapsed/remaining
        const pe = document.getElementById('prog_elapsed');
        const pr = document.getElementById('prog_remaining');
        const pb = document.getElementById('prog_bar');
        let elapsedSec = (typeof d.total_elapsed === 'number') ? d.total_elapsed : null;
        let remainingSec = (typeof d.total_remaining === 'number') ? d.total_remaining : null;
        if(d.repeat_total === null || d.repeat_total === undefined) { // infinite loop: only show elapsed
          elapsedSec = (typeof d.total_elapsed === 'number') ? d.total_elapsed : null;
          remainingSec = null;
        }
        if(pe) pe.textContent = (elapsedSec !== null ? elapsedSec + 's' : '');
        if(pr) pr.textContent = (remainingSec !== null ? remainingSec + 's' : '∞');
        if(pb) {
          let pct = 0;
          if(elapsedSec !== null && remainingSec !== null) {
            const total = Math.max(elapsedSec + remainingSec, 1);
            pct = Math.min(100, Math.max(0, (elapsedSec / total) * 100));
          } else if(elapsedSec !== null) {
            pct = 100;
          }
          pb.style.width = pct.toFixed(1) + '%';
        }
        const xval = tx();
        if(typeof d.events === 'number'){ evPoints.push({ x: xval, y: d.events }); evChart.update(); }
        if(typeof d.rate === 'number'){ ratePoints.push({ x: xval, y: d.rate }); rateChart.update(); }
        if(!d.running){ 
          document.getElementById('btn_m_stop').disabled = true;
          appendLog('Measurement stopped.');
        }
        // Update run history table
        if(d.runs){
          const table = document.getElementById('run_history');
          const tbody = table ? table.querySelector('tbody') : null;
          const thead = table ? table.querySelector('thead') : null;
          if(thead){
            const hdrText = thead.textContent || '';
            if(!/Counts/i.test(hdrText) || !/Rate/i.test(hdrText)){
              thead.innerHTML = '<tr><th>#</th><th>Timestamp</th><th>Repeat</th><th>Iteration</th><th>HV</th><th>Threshold</th><th>Duration(s)</th><th>Counts</th><th>Rate (1/s)</th><th>Run Info</th></tr>';
            }
          }
          if(tbody){
            tbody.innerHTML = '';
            d.runs.forEach((r, idx) => {
              const tr = document.createElement('tr');
              const ts = (typeof r.timestamp === 'number') ? new Date(r.timestamp * 1000).toLocaleTimeString() : '';
              const cells = [
                idx + 1,
                ts,
                r.repeat || '',
                r.iteration || '',
                (r.hv !== null && r.hv !== undefined) ? r.hv : '',
                (r.threshold !== null && r.threshold !== undefined) ? r.threshold : '',
                (r.duration !== null && r.duration !== undefined) ? Number(r.duration).toFixed(1) : '',
                (r.events !== null && r.events !== undefined) ? r.events : '',
                (r.rate !== null && r.rate !== undefined) ? Number(r.rate).toFixed(2) : '',
                r.run_info || ''
              ];
              cells.forEach(val => { const td = document.createElement('td'); td.textContent = String(val); tr.appendChild(td); });
              tbody.appendChild(tr);
            });
          }
        }
      } catch(e) { /* ignore */ }
    };
  }

  // Actions
  el('btn_hv_set').onclick = async function(){
    try {
      console.log('HV set clicked');
      appendLog('[HV] Sending SET VSET');
      const body = { value: Number(el('hv_value').value), device: el('hv_device').value, channel: el('hv_channel').value, baudrate: Number(el('hv_baud').value) };
      const res = await fetch('/hv/set', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
      const j = await res.json();
      appendLog('[HV] Response: ' + JSON.stringify(j));
      el('hv_result').innerHTML = 'Result: <code>' + JSON.stringify(j) + '</code>';
    } catch(err){ console.error('HV set error', err); appendLog('HV set error: ' + err); }
  };

  el('btn_hv_read').onclick = async function(){
    try {
      console.log('HV read clicked');
      appendLog('[HV] Sending MON VMON');
      const dev = encodeURIComponent(el('hv_device').value);
      const ch = encodeURIComponent(el('hv_channel').value);
      const baud = encodeURIComponent(el('hv_baud').value);
      const res = await fetch('/hv/read?device=' + dev + '&channel=' + ch + '&baudrate=' + baud);
      const j = await res.json();
      appendLog('[HV] Response: ' + JSON.stringify(j));
      el('hv_result').innerHTML = 'Result: <code>' + JSON.stringify(j) + '</code>';
    } catch(err){ console.error('HV read error', err); appendLog('HV read error: ' + err); }
  };

  el('btn_hv_send').onclick = async function(){
    try {
      console.log('HV send clicked');
      const valField = el('hv_val').value.trim();
      const body = { cmd: el('hv_cmd').value, par: el('hv_par').value, val: valField ? Number(valField) : null, device: el('hv_device').value, channel: el('hv_channel').value, baudrate: Number(el('hv_baud').value) };
      appendLog('[HV] Sending ' + body.cmd + ' ' + body.par + (valField ? (' val=' + valField) : ''));
      const res = await fetch('/hv/send', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
      const j = await res.json();
      appendLog('[HV] Response: ' + JSON.stringify(j));
      el('hv_result').innerHTML = 'Result: <code>' + JSON.stringify(j) + '</code>';
    } catch(err){ console.error('HV send error', err); appendLog('HV send error: ' + err); }
  };

  el('btn_m_start').onclick = async function(){
    try {
      console.log('Measure start clicked');
      hvPoints.length = 0; evPoints.length = 0; ratePoints.length = 0; hvChart.update(); evChart.update(); rateChart.update();
      const hvseqTxt = el('m_hvseq').value.trim();
      const thrseqTxt = el('m_thrseq').value.trim();
      const repeatTxt = el('m_repeat').value.trim();
      const body = {
        yaml: el('m_yaml').value,
        data_output: el('m_out').value,
        exe: el('m_exe').value,
        batch_mode: 2,
        max_events: Number(el('m_maxev').value),
        max_time: Number(el('m_maxt').value),
        hv_sequence: hvseqTxt ? hvseqTxt.split(',').map(s=>Number(s.trim())).filter(n=>!Number.isNaN(n)) : null,
        thresholds: thrseqTxt ? thrseqTxt.split(',').map(s=>Number(s.trim())).filter(n=>!Number.isNaN(n)) : null,
        repeat: repeatTxt ? Number(repeatTxt) : 1,
        source: el('m_source').value || null,
        scintillator: el('m_scint').value || null
      };
      const res = await fetch('/measure/start', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
      const j = await res.json();
      appendLog('measure/start response: ' + JSON.stringify(j));
      if(j.id){
        el('m_id').textContent = j.id;
        el('btn_m_stop').disabled = false;
        connectMeasure(j.id);
      }
    } catch(err){ console.error('Measure start error', err); appendLog('Measure start error: ' + err); }
  };

  el('btn_m_stop').onclick = async function(){
    try {
      console.log('Measure stop clicked');
      const id = el('m_id').textContent;
      if(!id || id === '(none)') return;
      const res = await fetch('/measure/stop/' + id, { method: 'POST' });
      const j = await res.json();
      appendLog('measure/stop response: ' + JSON.stringify(j));
      el('btn_m_stop').disabled = true;
    } catch(err){ console.error('Measure stop error', err); appendLog('Measure stop error: ' + err); }
  };

  // Clear chart buttons
  el('btn_clear_hv').onclick = function(){ hvPoints.length = 0; hvChart.update(); };
  el('btn_clear_events').onclick = function(){ evPoints.length = 0; evChart.update(); };
  el('btn_clear_rate').onclick = function(){ ratePoints.length = 0; rateChart.update(); };

  // Download run history
  const dlBtn = document.getElementById('btn_run_history_dl');
  if(dlBtn){
    dlBtn.onclick = async function(){
      const mid = document.getElementById('m_id').textContent;
      if(!mid || mid === '(none)') { appendLog('No active measurement ID to download history.'); return; }
      try {
        const res = await fetch('/measure/history/' + mid + '?format=csv');
        if(!res.ok){ appendLog('Download failed: HTTP ' + res.status); return; }
        const blob = await res.blob();
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = 'run_history_' + mid + '.csv';
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
        appendLog('Run history downloaded.');
      } catch(err){ appendLog('Download error: ' + err); }
    };
  }
})();
