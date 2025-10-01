#include "highscores.h"
#include <fstream>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

HighscoreManager::HighscoreManager(const std::string& file) : filename(file) {
    loadScores();
}

std::string HighscoreManager::getCurrentDate() {
    std::time_t now = std::time(nullptr);
    std::tm* local = std::localtime(&now);
    
    std::ostringstream oss;
    oss << std::setfill('0') 
        << std::setw(4) << (local->tm_year + 1900) << "-"
        << std::setw(2) << (local->tm_mon + 1) << "-"
        << std::setw(2) << local->tm_mday;
    
    return oss.str();
}

void HighscoreManager::sortScores() {
    std::sort(scores.begin(), scores.end(), 
        [](const HighscoreEntry& a, const HighscoreEntry& b) {
            return a.score > b.score;
        });
}

bool HighscoreManager::loadScores() {
    scores.clear();
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        // Si el archivo no existe, crear uno vacío con valores por defecto
        scores.push_back(HighscoreEntry(5000, "2025-01-01", "CLAUDIA"));
        scores.push_back(HighscoreEntry(4500, "2025-01-01", "ANTONIO"));
        scores.push_back(HighscoreEntry(4000, "2025-01-01", "SOFIA"));
        scores.push_back(HighscoreEntry(3500, "2025-01-01", "CARLOS"));
        scores.push_back(HighscoreEntry(3000, "2025-01-01", "MARIA"));
        saveScores();
        return true;
    }
    
    int score;
    std::string date, name;
    
    while (file >> score >> date >> name) {
        scores.push_back(HighscoreEntry(score, date, name));
    }
    
    file.close();
    sortScores();
    
    return true;
}

bool HighscoreManager::saveScores() {
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    
    for (const auto& entry : scores) {
        file << entry.score << " " << entry.date << " " << entry.name << "\n";
    }
    
    file.close();
    return true;
}

bool HighscoreManager::isHighscore(int score) {
    if (scores.size() < MAX_SCORES) {
        return true;
    }
    
    return score > scores.back().score;
}

bool HighscoreManager::addScore(int score, const std::string& name) {
    if (!isHighscore(score)) {
        return false;
    }
    
    std::string date = getCurrentDate();
    scores.push_back(HighscoreEntry(score, date, name));
    
    sortScores();
    
    // Mantener solo los mejores MAX_SCORES
    if (scores.size() > MAX_SCORES) {
        scores.resize(MAX_SCORES);
    }
    
    return saveScores();
}

void HighscoreManager::clear() {
    scores.clear();
    saveScores();
}