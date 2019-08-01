#ifndef QTSLIMEIDOSPRETTYPRINTER_H
#define QTSLIMEIDOSPRETTYPRINTER_H


#include <string>
#include <eidos_token.h>

class EidosScript;


// Generate a prettyprinted script string into parameter pretty, from the tokens and script supplied
bool Eidos_prettyprintTokensFromScript(const std::vector<EidosToken> &tokens, EidosScript &tokenScript, std::string &pretty);


#endif // QTSLIMEIDOSPRETTYPRINTER_H































