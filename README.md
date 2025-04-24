# Simulateur de Ransomware - Projet éducatif

Ce projet est un simulateur de ransomware créé **uniquement à des fins éducatives et de sensibilisation**. Il simule le comportement d'un ransomware typique sans causer de dommages réels au système.

⚠️ **AVERTISSEMENT** : Ce logiciel ne doit être utilisé que dans un environnement contrôlé et à des fins d'apprentissage. Toute utilisation malveillante est strictement interdite.

## Fonctionnalités

- Simulation complète du comportement d'un ransomware
- Chiffrement avec l'algorithme ChaCha20-Poly1305 via la bibliothèque libsodium
- Interface utilisateur graphique avec compte à rebours
- Exécution multi-thread pour optimiser les performances
- Communication avec un serveur C&C simulé
- Techniques anti-analyse de base (anti-débogage, détection de VM)
- Mécanismes de persistance (désactivés par défaut)

## Structure du Projet

Le projet est divisé en plusieurs fichiers pour une meilleure organisation et lisibilité :

- `main.h` - Fichier d'en-tête principal contenant toutes les déclarations
- `globals.c` - Définitions des variables globales
- `utils.c` - Fonctions utilitaires diverses
- `network.c` - Communication réseau (HTTP)
- `crypto.c` - Fonctions de chiffrement/déchiffrement
- `persistence.c` - Mécanismes de persistance
- `ui.c` - Interface utilisateur graphique
- `main.c` - Point d'entrée principal

## Prérequis

- Compilateur C compatible avec le standard C99
- Bibliothèque libsodium pour les fonctions cryptographiques
- Windows 7+
- CMake 3.10 ou supérieur

## Compilation

### Avec CMake

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### Avec MinGW/GCC

```bash
gcc -o ransomware.exe *.c -lsodium -lwinhttp -ladvapi32 -lshell32 -lgdi32 -luser32
```

## Fonctionnement

Le simulateur effectue les actions suivantes :

1. Crée des fichiers de test dans un répertoire spécifié
2. Génère une clé de chiffrement unique et un identifiant (UUID)
3. Chiffre les fichiers de test
4. Affiche une interface utilisateur avec une demande de rançon et un compte à rebours
5. Simule la vérification du paiement
6. Déchiffre les fichiers après le "paiement" simulé

## Sécurité

Ce simulateur inclut plusieurs mesures de sécurité :

- Le chiffrement est limité au répertoire `C:\Users\Public\Documents\TEST`
- Les fonctions de persistance sont désactivées par défaut
- L'interface admin requiert un mot de passe
- Les adresses Bitcoin sont factices

## Notes légales

Ce logiciel est fourni à des fins éducatives uniquement. L'auteur n'est pas responsable de toute utilisation abusive ou illégale. L'utilisation, la modification ou la distribution de ce code à des fins malveillantes est strictement interdite et peut constituer une infraction.

## Ressources additionnelles

Pour en savoir plus sur les ransomwares et la sécurité informatique :

- [ANSSI - Recommandations de sécurité](https://www.ssi.gouv.fr/guide/recommandations-de-securite-relatives-aux-ordiphones/)
- [US-CERT - Ransomware Guide](https://www.cisa.gov/stopransomware/ransomware-guide)
- [No More Ransom Project](https://www.nomoreransom.org/)
