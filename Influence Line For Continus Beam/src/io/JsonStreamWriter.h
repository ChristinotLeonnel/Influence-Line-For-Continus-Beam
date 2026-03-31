#pragma once
#ifndef __JSON_STREAM_WRITER__
#define __JSON_STREAM_WRITER__

// =============================================================================
//  JsonStreamWriter — Écriture JSON incrémentale (streaming)
// =============================================================================
//
//  Au lieu de sérialiser un tenseur 3D complet en mémoire (via nlohmann::json),
//  écrit le JSON directement dans le fichier travée par travée.
//
//  Format produit :   [ [span0_section0], [span0_section1], ...,
//                       [span1_section0], ... ]
//
//  Résultat identique à writeJsonFile(tensor3D, dir, filename) mais sans jamais
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
    explicit JsonStreamWriter(const std::filesystem::path& filepath)
    {
        std::filesystem::create_directories(filepath.parent_path());
        out_.open(filepath);
        if (!out_.is_open())
            throw std::runtime_error("JsonStreamWriter: cannot open " + filepath.string());
        out_ << '[';
        firstEntry_ = true;
    }

    JsonStreamWriter(const JsonStreamWriter&) = delete;
    JsonStreamWriter& operator=(const JsonStreamWriter&) = delete;
    JsonStreamWriter(JsonStreamWriter&&) = default;

    ~JsonStreamWriter()
    {
        if (out_.is_open()) {
            out_ << ']';
            out_.close();
        }
    }

    /** Écrit toutes les sections d'une travée. */
    void appendSpan(const std::vector<std::vector<double>>& data)
    {
        for (const auto& section : data) {
            if (!firstEntry_) out_ << ',';
            firstEntry_ = false;
            writeVec(section);
        }
        out_.flush();
    }

    /** Variante pour vecteurs 1D (abscisses, nœuds, ...). */
    void appendVec1D(const std::vector<double>& data)
    {
        if (!firstEntry_) out_ << ',';
        firstEntry_ = false;
        writeVec(data);
        out_.flush();
    }

    /** Ferme manuellement avant la destruction si nécessaire. */
    void finalize()
    {
        if (out_.is_open()) {
            out_ << ']';
            out_.close();
        }
    }

private:
    std::ofstream out_;
    bool          firstEntry_ = true;   // ex first_span_

    void writeVec(const std::vector<double>& v)
    {
        out_ << '[';
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) out_ << ',';
            out_ << v[i];
        }
        out_ << ']';
    }
};

#endif // __JSON_STREAM_WRITER__