"""
setup_pure.py — Script de construction de la wheel pure-Python (py3-none-any)
et de la source distribution pour Tsaraloha.

Permet l'installation universelle sans aucun compilateur C++ :
- Smartphones (Android / Pydroid 3 / Termux, iOS / Pyto / Carnets)
- Environnements restreints ou architectures ARM/Linux/macOS
"""

import pathlib
from setuptools import setup, find_packages

here = pathlib.Path(__file__).parent.resolve()
readme = (here / "README.md").read_text(encoding="utf-8") if (here / "README.md").exists() else ""

setup(
    name="Tsaraloha",
    version="1.1.0",
    description="Tsaraloha : librairie de calcul de structures et poutres continues (lignes d'influence, enveloppes de charges)",
    long_description=readme,
    long_description_content_type="text/markdown",
    author="Tsaraloha",
    url="https://github.com/ChristinotLeonnel/Influence-Line-For-Continus-Beam",
    project_urls={
        "Source": "https://github.com/ChristinotLeonnel/Influence-Line-For-Continus-Beam",
        "Bug Tracker": "https://github.com/ChristinotLeonnel/Influence-Line-For-Continus-Beam/issues",
    },
    packages=find_packages(where="python"),
    package_dir={"": "python"},
    package_data={
        "Tsaraloha": ["py.typed"],
        "Tsaraloha.LIPoutreContinue": ["*.pyi", "py.typed"],
    },
    python_requires=">=3.9",
    install_requires=[
        "numpy>=1.20",
    ],
    extras_require={
        "plot": ["matplotlib>=3.5"],
        "report": ["weasyprint"],
        "all": ["matplotlib>=3.5", "weasyprint"],
    },
    classifiers=[
        "Development Status :: 5 - Production/Stable",
        "Intended Audience :: Science/Research",
        "Topic :: Scientific/Engineering",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Programming Language :: Python :: 3.13",
        "Operating System :: OS Independent",
    ],
)
