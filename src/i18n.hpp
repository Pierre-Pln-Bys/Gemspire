#ifndef SPLENDOR_I18N_HPP
#define SPLENDOR_I18N_HPP

#include <string>
#include <vector>

// Langue des textes du jeu. Le code est écrit en anglais ; la traduction
// française de chaque texte est dans la table de i18n.cpp.
enum class Lang { English, French };

// Texte `english` dans la langue `lang` (le texte anglais lui-même s'il n'a
// pas de traduction : noms de joueurs, nombres...)
std::string tr(Lang lang, const std::string& english);

// Pareil, puis remplace {0}, {1}... par `values` :
// tr(lang, "{0}'s turn", {name}) -> « Au tour de Alice » en français
std::string tr(Lang lang, const std::string& english, const std::vector<std::string>& values);

// "fr", "fr-FR", "fr_FR.UTF-8" -> français ; tout le reste -> anglais
Lang lang_from_code(const std::string& code);

// Code court de la langue ("en", "fr"), pour les fichiers de réglages
std::string lang_code(Lang lang);

// Langue du système (variables LANGUAGE, LC_ALL, LC_MESSAGES, LANG) ;
// anglais si aucune n'est définie (cas habituel sous Windows)
Lang system_lang();

#endif
