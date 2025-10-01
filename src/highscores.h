#ifndef HIGHSCORES_H
#define HIGHSCORES_H

#include <string>
#include <vector>

struct HighscoreEntry {
    int score;
    std::string date;
    std::string name;
    
    HighscoreEntry(int s = 0, const std::string& d = "", const std::string& n = "")
        : score(s), date(d), name(n) {}
};

class HighscoreManager {
private:
    std::vector<HighscoreEntry> scores;
    std::string filename;
    const int MAX_SCORES = 10;

    std::string getCurrentDate();
    void sortScores();

public:
    HighscoreManager(const std::string& file = "highscores.txt");
    
    bool loadScores();
    bool saveScores();
    bool addScore(int score, const std::string& name);
    bool isHighscore(int score);
    const std::vector<HighscoreEntry>& getScores() const { return scores; }
    void clear();
};

#endif // HIGHSCORES_H