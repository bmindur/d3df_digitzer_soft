(function(){
  const el = id => document.getElementById(id);
  const hvPoints = [];
  const evPoints = [];
  const ratePoints = [];
  const t0 = Date.now();
  const tx = () => (Date.now() - t0) / 1000.0;
  function appendLog(msg){ const area = el('log'); area.textContent += msg + '\n'; area.scrollTop = area.scrollHeight; }

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
    wsHV.onopen = function(){ setHVStatus(true); };
    wsHV.onmessage = function(ev){
      try {
        const data = JSON.parse(ev.data);
        if(data.hv !== undefined){
          const y = Number(data.hv);
          if(!Number.isNaN(y)){
            hvPoints.push({ x: tx(), y: y });
            hvChart.update();
          }
        }
        if(data.error){ appendLog('[HV] ' + data.error); }
      } catch(e) { /* ignore */ }
    };
    wsHV.onclose = function(){ setHVStatus(false); if(hvMonitoring){ setTimeout(openHV, 3000); } };
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
        if(d.last_line){ appendLog(d.last_line); }
        if(d.progress_line){ appendLog(d.progress_line); }
        const xval = tx();
        if(typeof d.events === 'number'){ evPoints.push({ x: xval, y: d.events }); evChart.update(); }
        if(typeof d.rate === 'number'){ ratePoints.push({ x: xval, y: d.rate }); rateChart.update(); }
        if(!d.running){ el('btn_m_stop').disabled = true; }
      } catch(e) { /* ignore */ }
    };
  }

  // Actions
  el('btn_hv_set').onclick = async function(){
    try {
      console.log('HV set clicked');
      const body = { value: Number(el('hv_value').value), device: el('hv_device').value, channel: el('hv_channel').value, baudrate: Number(el('hv_baud').value) };
      const res = await fetch('/hv/set', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
      const j = await res.json();
      el('hv_result').innerHTML = 'Result: <code>' + JSON.stringify(j) + '</code>';
    } catch(err){ console.error('HV set error', err); appendLog('HV set error: ' + err); }
  };

  el('btn_hv_read').onclick = async function(){
    try {
      console.log('HV read clicked');
      const dev = encodeURIComponent(el('hv_device').value);
      const ch = encodeURIComponent(el('hv_channel').value);
      const baud = encodeURIComponent(el('hv_baud').value);
      const res = await fetch('/hv/read?device=' + dev + '&channel=' + ch + '&baudrate=' + baud);
      const j = await res.json();
      el('hv_result').innerHTML = 'Result: <code>' + JSON.stringify(j) + '</code>';
    } catch(err){ console.error('HV read error', err); appendLog('HV read error: ' + err); }
  };

  el('btn_hv_send').onclick = async function(){
    try {
      console.log('HV send clicked');
      const valField = el('hv_val').value.trim();
      const body = { cmd: el('hv_cmd').value, par: el('hv_par').value, val: valField ? Number(valField) : null, device: el('hv_device').value, channel: el('hv_channel').value, baudrate: Number(el('hv_baud').value) };
      const res = await fetch('/hv/send', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
      const j = await res.json();
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
        batch_mode: Number(el('m_batch').value),
        max_events: Number(el('m_maxev').value),
        max_time: Number(el('m_maxt').value),
        hv_sequence: hvseqTxt ? hvseqTxt.split(',').map(s=>Number(s.trim())).filter(n=>!Number.isNaN(n)) : null,
        thresholds: thrseqTxt ? thrseqTxt.split(',').map(s=>Number(s.trim())).filter(n=>!Number.isNaN(n)) : null,
        repeat: repeatTxt ? Number(repeatTxt) : 1
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
})();
