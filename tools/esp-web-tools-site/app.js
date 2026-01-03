(() => {
  const input = document.getElementById('boardFilter');
  const container = document.getElementById('boards');
  const count = document.getElementById('boardCount');

  if (!input || !container || !count) return;

  const cards = Array.from(container.querySelectorAll('[data-board]'));

  function update() {
    const q = (input.value || '').trim().toLowerCase();
    let visible = 0;

    for (const el of cards) {
      const name = (el.getAttribute('data-board') || '').toLowerCase();
      const chip = (el.getAttribute('data-chip') || '').toLowerCase();
      const match = !q || name.includes(q) || chip.includes(q);
      el.style.display = match ? '' : 'none';
      if (match) visible++;
    }

    count.textContent = `${visible} / ${cards.length} boards`;
  }

  input.addEventListener('input', update);
  update();
})();
