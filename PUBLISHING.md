# Publier Tsaraloha sur PyPI (Windows uniquement)

Ce document décrit la marche à suivre **une seule fois**, puis le geste
répété à chaque nouvelle version. Le pipeline (`.github/workflows/wheels.yml`)
est déjà en place ; il ne manque que la configuration côté GitHub/PyPI, que
je ne peux pas faire à votre place (comptes personnels).

## 1) Créer le projet sur PyPI (une seule fois)

1. Créez un compte sur https://pypi.org/account/register/ si vous n'en avez
   pas.
2. Allez sur https://pypi.org/manage/account/publishing/ (« Trusted
   Publishers » — cette page permet d'autoriser GitHub Actions à publier
   **sans mot de passe ni token**, c'est la méthode recommandée en 2026,
   celle qu'utilise numpy).
3. Remplissez :
   - **PyPI project name** : `Tsaraloha`
   - **Owner** : `ChristinotLeonnel`
   - **Repository name** : `Influence-Line-For-Continus-Beam`
   - **Workflow name** : `wheels.yml`
   - **Environment name** : `pypi`
4. Validez. PyPI réserve alors le nom `Tsaraloha` et fera confiance à ce
   workflow précis pour publier — même avant le tout premier upload.

## 2) (Recommandé) Protéger l'environnement GitHub

Dans votre repo GitHub : **Settings → Environments → New environment**,
nommez-le `pypi`, et ajoutez-vous comme « required reviewer ». Résultat :
chaque publication sur PyPI demandera une validation manuelle dans GitHub
Actions avant de partir — évite une publication accidentelle sur un simple
tag mal placé.

## 3) Publier une version

```bash
# 1. Mettez à jour le numéro de version dans pyproject.toml (project.version)
# 2. Committez, puis taguez :
git commit -am "Version 1.0.0"
git tag v1.0.0
git push origin main --tags
```

Le tag `v1.0.0` déclenche automatiquement `.github/workflows/wheels.yml` :
- construction des wheels Windows x64 pour Python 3.9 à 3.13,
- construction du sdist,
- publication sur PyPI (après validation manuelle si vous avez activé
  l'étape 2).

Suivez la progression dans l'onglet **Actions** du repo GitHub. Une fois
terminé :

```bash
pip install Tsaraloha
python -c "import Tsaraloha.LIPoutreContinue as m; print(m.__file__)"
```

fonctionne sur n'importe quel PC Windows, sans compilateur C++ ni CMake
installé — exactement comme `pip install numpy`. Les dépendances
(`matplotlib`, `numpy`) sont installées automatiquement.

## 4) Tester sans publier (recommandé avant le premier vrai tag)

Le workflow tourne aussi sur chaque pull request et sur déclenchement
manuel (bouton « Run workflow » dans l'onglet Actions), ce qui construit
les wheels **sans** les publier — utile pour vérifier que la compilation
passe avant de taguer une vraie version.

## Rappel — licence « Proprietary »

`pyproject.toml` déclare `license = "Proprietary"`. PyPI n'exige pas que le
code soit open-source : publier là-bas rend seulement le **binaire**
(wheels) et le **sdist** téléchargeables publiquement par n'importe qui —
la licence, elle, continue d'interdire légalement la redistribution/
modification par vos utilisateurs si c'est ce que vous voulez.
