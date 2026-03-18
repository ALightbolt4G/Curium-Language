// UI Elements
const loginSection = document.getElementById('login-section');
const dlSection = document.getElementById('downloader-section');
const loginForm = document.getElementById('login-form');
const dlForm = document.getElementById('download-form');
const errorMsg = document.getElementById('login-error');
const userDisplay = document.getElementById('user-display');
const statusMsg = document.getElementById('download-status');
const dlBtn = document.getElementById('download-btn');
const btnText = document.getElementById('btn-text');
const btnSpinner = document.getElementById('btn-spinner');

// Authentication
loginForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    const username = document.getElementById('username').value;
    const password = document.getElementById('password').value;

    try {
        const response = await fetch('/api/login', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ username, password })
        });
        
        const data = await response.json();
        
        if (data.status === 'success') {
            userDisplay.textContent = username;
            
            // Trigger smooth transition
            loginSection.classList.remove('active');
            setTimeout(() => {
                dlSection.classList.add('active');
            }, 300); // Wait for fade out
        } else {
            errorMsg.textContent = data.message || 'Invalid credentials';
            // Shake effect
            loginSection.style.animation = 'shake 0.5s cubic-bezier(.36,.07,.19,.97) both';
            setTimeout(() => { loginSection.style.animation = '' }, 500);
        }
    } catch (err) {
        errorMsg.textContent = 'Server connection failed';
    }
});

function logout() {
    dlSection.classList.remove('active');
    setTimeout(() => {
        loginForm.reset();
        errorMsg.textContent = '';
        loginSection.classList.add('active');
        document.getElementById('youtube-url').value = '';
        statusMsg.textContent = '';
    }, 300);
}

// Downloader Logic
dlForm.addEventListener('submit', async (e) => {
    e.preventDefault();
    const url = document.getElementById('youtube-url').value;

    if (!url.includes('youtube.com') && !url.includes('youtu.be')) {
        showStatus('Please enter a valid YouTube URL', 'error');
        return;
    }

    // Set loading state
    btnText.textContent = 'Downloading...';
    btnSpinner.classList.remove('hidden');
    dlBtn.disabled = true;
    showStatus('Requesting download from server...', 'info');

    try {
        const response = await fetch('/api/download', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ url: url })
        });

        const data = await response.json();

        if (data.status === 'success') {
            showStatus('✅ Video downloaded successfully!', 'success');
        } else {
            showStatus('❌ ' + (data.message || 'Download failed'), 'error');
        }
    } catch (e) {
        showStatus('❌ Server communication error', 'error');
    } finally {
        // Reset loading state
        btnText.textContent = 'Download Video ⬇️';
        btnSpinner.classList.add('hidden');
        dlBtn.disabled = false;
        document.getElementById('youtube-url').value = '';
    }
});

function showStatus(text, type) {
    statusMsg.textContent = text;
    statusMsg.className = 'status-msg ' + type;
}

// Global keyframes injection for shake
const style = document.createElement('style');
style.innerHTML = `
@keyframes shake {
  10%, 90% { transform: translate3d(-1px, 0, 0); }
  20%, 80% { transform: translate3d(2px, 0, 0); }
  30%, 50%, 70% { transform: translate3d(-4px, 0, 0); }
  40%, 60% { transform: translate3d(4px, 0, 0); }
}
`;
document.head.appendChild(style);
