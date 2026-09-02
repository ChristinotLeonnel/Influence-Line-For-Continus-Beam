# Publier Tsaraloha sur PyPI (comme numpy)

Ce document décrit la marche à suivre **une seule fois**, puis le geste
répété à chaque nouvelle version. Le pipeline (`.github/workflows/wheels.yml`)
est déjà en place ; il ne manque que la configuration côté GitHub/PyPI, que
je ne peux pas faire à votre place (comptes personnels).

## 1) Créer le dépôt GitHub

```bash
cd /chemin/vers/le/projet
git init
git add .
git commit -m "Version initiale"
gh repo create <votre-org>/Tsaraloha --public --source=. --push
# ou créez le repo sur github.com puis :
#   git remote add origin https://github.com/<votre-org>/Tsaraloha.git
#   git push -u origin main
```

## 2) Créer le projet sur PyPI (une seule fois)

1. Créez un compte sur https://pypi.org/account/register/ si vous n'en avez
   pas.
2. Allez sur https://pypi.org/manage/account/publishing/ ("Trusted
   Publishers" — cette page permet d'autoriser GitHub Actions à publier
   **sans mot de passe ni token**, c'est la méthode recommandée en 2026,
   celle qu'utilise numpy).
3. Remplissez :
   - **PyPI project name** : `Tsaraloha`
   - **Owner** : `<votre-org>` (nom du compte/organisation GitHub)
   - **Repository name** : `Tsaraloha` (ou le nom réel de votre repo)
   - **Workflow name** : `wheels.yml`
   - **Environment name** : `pypi` (doit correspondre à `environment.name`
     dans `.github/workflows/wheels.yml` — déjà réglé sur `pypi`)
4. Validez. PyPI réserve alors le nom `Tsaraloha` et fera confiance à ce
   workflow précis pour publier — même avant le tout premier upload.

## 3) (Recommandé) Protéger l'environnement GitHub

Dans votre repo GitHub : **Settings → Environments → New environment**,
nommez-le `pypi`, et ajoutez-vous (ou votre équipe) comme "required
reviewer". Résultat : chaque publication sur PyPI demandera une validation
manuelle dans GitHub Actions avant de partir — évite une publication
accidentelle sur un simple tag mal placé.

## 4) Publier une version

```bash
# 1. Mettez à jour le numéro de version dans pyproject.toml (project.version)
# 2. Committez, puis taguez :
git commit -am "Version 1.0.0"
git tag v1.0.0
git push origin main --tags
```

Le tag `v1.0.0` déclenche automatiquement `.github/workflows/wheels.yml` :
- construction des wheels Windows / Linux / macOS (Intel + Apple Silicon)
  pour Python 3.9 à 3.13,
- construction du sdist,
- publication sur PyPI (après validation manuelle si vous avez activé
  l'étape 3).

Suivez la progression dans l'onglet **Actions** du repo GitHub. Une fois
terminé :

```bash
pip install Tsaraloha
python -c "import Tsaraloha.LIPoutreContinue as m; print(m.__file__)"
```

fonctionne sur n'importe quel PC Windows, sans compilateur C++ ni CMake
installé — exactement comme `pip install numpy`. Les dépendances
(`matplotlib`, `numpy`) sont installées automatiquement.

## 5) Tester sans publier (recommandé avant le premier vrai tag)

Le workflow tourne aussi sur chaque pull request et sur déclenchement
manuel (bouton "Run workflow" dans l'onglet Actions), ce qui construit
toutes les wheels **sans** les publier — utile pour vérifier que la
compilation passe sur les 4 plateformes avant de taguer une vraie version.

Vous pouvez aussi tester la publication sur **TestPyPI** d'abord (pratique
courante) : créez un second Trusted Publisher sur
https://test.pypi.org/manage/account/publishing/ avec les mêmes infos, puis
dupliquez temporairement le job `publish_pypi` en pointant
`repository-url: https://test.pypi.org/legacy/` et
`environment.url: https://test.pypi.org/p/Tsaraloha`.

## Rappel — licence "Proprietary"

`pyproject.toml` déclare `license = "Proprietary"`. PyPI n'exige pas que le
code soit open-source : publier là-bas rend seulement le **binaire**
(wheels) et le **sdist** téléchargeables publiquement par n'importe qui —
la licence, elle, continue d'interdire légalement la redistribution/
modification par vos utilisateurs si c'est ce que vous voulez. Si vous
préférez qu'aucun tiers ne puisse même télécharger le paquet, il faudra
plutôt un index privé (Azure Artifacts, Cloudsmith, ...) — on peut basculer
dessus plus tard sans toucher au pipeline de build, juste à l'étape
`publish_pypi`.
