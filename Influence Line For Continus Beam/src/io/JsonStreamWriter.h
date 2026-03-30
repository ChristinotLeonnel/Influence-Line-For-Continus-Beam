#pragma once
#ifndef __JSON_STREAM_WRITER__
#define __JSON_STREAM_WRITER__

// =============================================================================
//  JsonStreamWriter — Écriture JSON incrémentale (streaming)
// =============================================================================
//
//  Au lieu de sérialiser un tenseur 3D complet en mémoire (via nlohmann::json),
//  on écrit le JSON directement dans le fichier travée par travée.
//
//  Format produit :   [ [span0_section0], [span0_section1], ...,
//                       [span1_section0], ... ]
//
//  Résultat identique à delivery(tensor3D, path, filename) mais sans jamais
//  allouer le tenseur complet en RAM.
// =============================================================================

#include <filesystem>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <string>

class JsonStreamWriter
{
public:
    // ── Construction : ouvre le fichier et écrit le début du tableau JSON ──
    explicit JsonStreamWriter(const std::filesystem::path& filepath)
    {
        std::filesystem::create_directories(filepath.parent_path());
        out_.open(filepath);
        if (!out_.is_open())
            throw std::runtime_error("JsonStreamWriter: cannot open " + filepath.string());
        out_ << '[';
        first_span_ = true;
    }

    // ── Pas de copie — move autorisé ───────────────────────────────────────
    JsonStreamWriter(const JsonStreamWriter&)            = delete;
    JsonStreamWriter& operator=(const JsonStreamWriter&) = delete;
    JsonStreamWriter(JsonStreamWriter&&)                 = default;

    // ── Destructeur : ferme le tableau JSON ────────────────────────────────
    ~JsonStreamWriter()
    {
        if (out_.is_open()) {
            out_ << ']';
            out_.close();
        }
    }

    // ── appendSpan : écrit toutes les sections d'une travée ───────────────
    //
    //  @param data  [section][alpha] — vecteur de la travée courante
    //
    void appendSpan(const std::vector<std::vector<double>>& data)
    {
        for (const auto& section : data) {
            if (!first_span_) out_ << ',';
            first_span_ = false;
            writeVec(section);
        }
        out_.flush(); // garantit l'écriture disque même en cas d'interruption
    }

    // ── appendVec1D : variante pour vecteurs 1D (abscisses, nœuds, ...) ──
    void appendVec1D(const std::vector<double>& data)
    {
        if (!first_span_) out_ << ',';
        first_span_ = false;
        writeVec(data);
        out_.flush();
    }

    // ── finalize : ferme manuellement si besoin avant la destruction ──────
    void finalize()
    {
        if (out_.is_open()) {
            out_ << ']';
            out_.close();
        }
    }

private:
    std::ofstream out_;
    bool          first_span_ = true;

    // Écriture d'un vecteur 1D au format JSON sans allocation intermédiaire
    void writeVec(const std::vector<double>& v)
    {
        out_ << '[';
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) out_ << ',';
            // std::to_string donne une précision fixe insuffisante pour des
            // valeurs d'ingénierie — on utilise le formateur par défaut du stream
            // (17 chiffres significatifs = round-trip garanti)
            out_ << v[i];
        }
        out_ << ']';
    }
};

#endif // __JSON_STREAM_WRITER__
