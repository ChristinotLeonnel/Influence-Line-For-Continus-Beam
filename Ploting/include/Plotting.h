#pragma once
/**
 * Ploting.h
 * Interface publique de la librairie de visualisation.
 * Compilée en Ploting.lib — linkée par Aplication.cpp.
 */

#include <string>

namespace plotting {

	/**
	 * Lance le pipeline complet de génération des plots et animations.
	 *
	 * @param configPath  Chemin racine "Influence Line" lu depuis path.json.
	 *                    Transmis directement à influence_line_dir() via
	 *                    la variable d'environnement interne.
	 */
	void run(const std::string& configPath);

} // namespace plotting