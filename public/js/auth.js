/* ============================================================================
   auth.js — sign-in and registration pages
   All validation is duplicated server-side; this only improves the experience.
   ========================================================================== */
'use strict';

(function () {
  const errorBox = document.getElementById('auth-error');
  const errorText = document.getElementById('auth-error-text');
  const noticeBox = document.getElementById('auth-notice');

  function showError(message) {
    if (!errorBox) return;
    errorText.textContent = message;
    errorBox.classList.remove('hidden');
    errorBox.scrollIntoView({ block: 'nearest', behavior: 'smooth' });
  }

  function clearError() {
    if (errorBox) errorBox.classList.add('hidden');
    document.querySelectorAll('[data-err]').forEach((n) => {
      n.classList.add('hidden');
      n.textContent = '';
    });
    document.querySelectorAll('.invalid').forEach((n) => n.classList.remove('invalid'));
  }

  function fieldError(name, message) {
    const node = document.querySelector(`[data-err="${name}"]`);
    const input = document.getElementById(name);
    if (node) { node.textContent = message; node.classList.remove('hidden'); }
    if (input) input.classList.add('invalid');
  }

  function busy(button, isBusy, label) {
    if (!button) return;
    button.disabled = isBusy;
    button.classList.toggle('is-busy', isBusy);
    if (label) button.textContent = isBusy ? label : button.dataset.label || button.textContent;
  }

  function safeNext(raw) {
    if (!raw) return null;
    // only allow same-site relative paths
    if (raw.charAt(0) !== '/' || raw.charAt(1) === '/') return null;
    return raw;
  }

  /* ------------------------------------------------------------------ login */
  const loginForm = document.getElementById('login-form');
  if (loginForm) {
    const params = new URLSearchParams(location.search);
    if (params.get('signedout')) {
      if (noticeBox) {
        noticeBox.textContent = 'You have been signed out.';
        noticeBox.classList.remove('hidden');
      }
    }
    if (params.get('expired')) {
      if (noticeBox) {
        noticeBox.textContent = 'Your session expired. Please sign in again.';
        noticeBox.classList.remove('hidden');
      }
    }
    const next = safeNext(params.get('next'));

    const btn = document.getElementById('login-btn');
    btn.dataset.label = btn.textContent;

    loginForm.addEventListener('submit', async (event) => {
      event.preventDefault();
      clearError();
      const username = document.getElementById('username').value.trim();
      const password = document.getElementById('password').value;

      let invalid = false;
      if (!username) { fieldError('username', 'Username is required.'); invalid = true; }
      if (!password) { fieldError('password', 'Password is required.'); invalid = true; }
      if (invalid) return;

      busy(btn, true, 'Signing in…');
      const res = await API.post('/api/auth/login', { username, password });
      busy(btn, false);

      if (!res.ok) {
        showError(res.data.error || 'Sign-in failed. Please try again.');
        return;
      }
      const target = next || res.data.redirect || (res.data.user.isAdmin ? '/admin.html' : '/person.html');
      location.replace(target);
    });
  }

  /* --------------------------------------------------------------- register */
  const registerForm = document.getElementById('register-form');
  if (registerForm) {
    const btn = document.getElementById('register-btn');
    btn.dataset.label = btn.textContent;

    registerForm.addEventListener('submit', async (event) => {
      event.preventDefault();
      clearError();

      const username = document.getElementById('username').value.trim();
      const password = document.getElementById('password').value;
      const confirm = document.getElementById('confirm').value;

      let invalid = false;
      if (username.length < 3 || username.length > 24) {
        fieldError('username', 'Username must be between 3 and 24 characters.');
        invalid = true;
      } else if (!/^[A-Za-z0-9._-]+$/.test(username)) {
        fieldError('username', "Use only letters, digits, '_', '.' and '-'.");
        invalid = true;
      }
      if (password.length < 6) {
        fieldError('password', 'Password must be at least 6 characters.');
        invalid = true;
      }
      if (password !== confirm) {
        fieldError('confirm', 'Passwords do not match.');
        invalid = true;
      }
      if (invalid) return;

      busy(btn, true, 'Creating account…');
      const res = await API.post('/api/auth/register', { username, password });
      busy(btn, false);

      if (!res.ok) {
        showError(res.data.error || 'Registration failed.');
        return;
      }
      location.replace(res.data.redirect || '/person.html');
    });
  }
})();
