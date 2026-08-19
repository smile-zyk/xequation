#include "scanner.h"

#include <cstring>
#include <unordered_map>
#include <utility>

namespace rel
{
    namespace
    {
            constexpr char kEof = '\0';

            bool is_digit(char c) { return c >= '0' && c <= '9'; }
            bool is_octal_digit(char c) { return c >= '0' && c <= '7'; }
            bool is_hex_digit(char c)
            {
                return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
            }
            bool is_alpha(char c)
            {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
            }
            bool is_alnum(char c) { return is_alpha(c) || is_digit(c); }

            // Keyword table (Spec 1.1). Built-in constants such as PI / e / ln10
            // are NOT keywords; they live in this map's complement and are
            // resolved at evaluation time (Spec 1.7 rule 4).
            const std::unordered_map<std::string, TokenType>& keyword_table()
            {
                static const std::unordered_map<std::string, TokenType> kTable = {
                    {"if",        TokenType::KW_IF},
                    {"then",      TokenType::KW_THEN},
                    {"elseif",    TokenType::KW_ELSEIF},
                    {"else",      TokenType::KW_ELSE},
                    {"AND",       TokenType::KW_AND},
                    {"OR",        TokenType::KW_OR},
                    {"NOT",       TokenType::KW_NOT},
                    {"EQUALS",    TokenType::KW_EQUALS},
                    {"NOTEQUALS", TokenType::KW_NOTEQUALS},
                    {"TRUE",      TokenType::KW_TRUE},
                    {"FALSE",     TokenType::KW_FALSE},
                };
                return kTable;
            }

            // PREDEF_SCALED_UNIT (Spec 1.5), longest-first so the linear scan
            // below returns the longest matching prefix.
            const char* const kPredefScaledUnits[] = {
                "mils", "PHz", "nmi", "mil", "cm", "dB", "ft", "in", "mi"
            };
            const std::size_t kPredefScaledUnitsCount =
                sizeof(kPredefScaledUnits) / sizeof(kPredefScaledUnits[0]);

            // UNIT (Spec 1.5), longest-first for the same reason.
            const char* const kUnits[] = {
                "meters", "metres", "meter", "metre",
                "Ohms",   "Ohm",    "sec",   "Hz",
                "S", "F", "H", "V", "A", "W"
            };
            const std::size_t kUnitsCount =
                sizeof(kUnits) / sizeof(kUnits[0]);

            // SCALE_FACTOR (Spec 1.5) is always a single character.
            bool is_scale_factor_char(char c)
            {
                switch (c)
                {
                    case 'T': case 'G': case 'M': case 'K': case 'k':
                    case '_': case 'm': case 'u': case 'n': case 'p':
                    case 'f': case 'a':
                        return true;
                    default:
                        return false;
                }
            }

            std::size_t match_longest_prefix(const char* input, std::size_t input_size,
                                             const char* const* table,
                                             std::size_t table_size)
            {
                for (std::size_t i = 0; i < table_size; ++i)
                {
                    const char* candidate = table[i];
                    std::size_t candidate_len = std::strlen(candidate);
                    if (input_size >= candidate_len &&
                        std::memcmp(input, candidate, candidate_len) == 0)
                    {
                        return candidate_len;
                    }
                }
                return 0;
            }

            std::size_t match_predef_scaled_unit(const char* s, std::size_t n)
            {
                return match_longest_prefix(s, n, kPredefScaledUnits,
                                            kPredefScaledUnitsCount);
            }

            std::size_t match_unit(const char* s, std::size_t n)
            {
                return match_longest_prefix(s, n, kUnits, kUnitsCount);
            }
        }  // namespace

        Scanner::Scanner(std::string source, int initial_line)
            : source_(std::move(source)), line_(initial_line), start_line_(initial_line)
        {
        }

        ScanResult Scanner::Scan()
        {
            while (!is_at_end())
            {
                // Spec 1.6: WS = [ \t\f\r\n]+. Skip whitespace between tokens.
                char c = peek();
                if (c == ' ' || c == '\t' || c == '\f' || c == '\r')
                {
                    advance();
                    continue;
                }
                if (c == '\n')
                {
                    advance();
                    line_++;
                    line_start_ = current_;
                    continue;
                }

                mark_token_start();
                scan_one_token();
            }

            // Synthetic END_OF_INPUT (Spec 1.7 rule 5).
            mark_token_start();
            emit(TokenType::END_OF_INPUT, "");
            return {std::move(tokens_), std::move(errors_)};
        }

        // ---------------------------------------------------------------------
        // Cursor primitives
        // ---------------------------------------------------------------------

        bool Scanner::is_at_end() const
        {
            return current_ >= source_.size();
        }

        char Scanner::peek(std::size_t offset) const
        {
            const std::size_t pos = current_ + offset;
            return (pos >= source_.size()) ? kEof : source_[pos];
        }

        char Scanner::advance()
        {
            return source_[current_++];
        }

        bool Scanner::match(char expected)
        {
            if (peek() != expected) return false;
            ++current_;
            return true;
        }

        void Scanner::mark_token_start()
        {
            start_ = current_;
            start_line_ = line_;
            start_col_ = static_cast<int>(current_ - line_start_) + 1;
        }

        // ---------------------------------------------------------------------
        // Token emission
        // ---------------------------------------------------------------------

        void Scanner::emit(TokenType type)
        {
            emit(type, source_.substr(start_, current_ - start_));
        }

        void Scanner::emit(TokenType type, std::string lexeme)
        {
            tokens_.push_back(Token{type, std::move(lexeme), start_line_, start_col_});
        }

        void Scanner::add_error(const std::string& message)
        {
            Error err;
            err.kind    = ErrorKind::Lexical;
            err.line    = start_line_;
            err.column  = start_col_;
            err.message = message;
            errors_.push_back(std::move(err));
        }

        // ---------------------------------------------------------------------
        // Dispatch
        // ---------------------------------------------------------------------

        void Scanner::scan_one_token()
        {
            char c = peek();

            // Identifier / keyword - the helper does its own consumption.
            if (is_alpha(c))
            {
                scan_identifier_or_keyword();
                return;
            }
            // Numeric literal - same idea.
            if (is_digit(c))
            {
                scan_numeric_literal();
                return;
            }

            // Everything else: consume the leading character now.
            advance();
            switch (c)
            {
                // --- Delimiters (Spec 1.2) ----------------------------------
                case '(': emit(TokenType::LPAREN);   return;
                case ')': emit(TokenType::RPAREN);   return;
                case '[': emit(TokenType::LBRACKET); return;
                case ']': emit(TokenType::RBRACKET); return;
                case '{': emit(TokenType::LBRACE);   return;
                case '}': emit(TokenType::RBRACE);   return;
                case ',': emit(TokenType::COMMA);    return;

                // --- Single-character operators -----------------------------
                case '+': emit(TokenType::OP_ADD);   return;
                case '-': emit(TokenType::OP_SUB);   return;
                case '/': emit(TokenType::OP_DIV);   return;
                case '%': emit(TokenType::OP_MOD);   return;
                case '^': emit(TokenType::OP_BXOR);  return;
                case '~': emit(TokenType::OP_BNOT);  return;
                case '?': emit(TokenType::OP_QMARK); return;

                // --- Two-character operators with single-char prefixes ------
                // Spec 1.7 rule 1: maximal munch. ** > *, :: > :, etc.
                case '*':
                    emit(match('*') ? TokenType::OP_POW : TokenType::OP_MUL);
                    return;
                case ':':
                    emit(match(':') ? TokenType::OP_SEQ : TokenType::OP_COLON);
                    return;
                case '<':
                    if (match('<')) { emit(TokenType::OP_SHL); return; }
                    if (match('=')) { emit(TokenType::OP_LE);  return; }
                    emit(TokenType::OP_LT);
                    return;
                case '>':
                    if (match('>')) { emit(TokenType::OP_SHR); return; }
                    if (match('=')) { emit(TokenType::OP_GE);  return; }
                    emit(TokenType::OP_GT);
                    return;
                case '=':
                    // The spec has no bare '=' token. Lone '=' is a lexical error.
                    if (match('=')) { emit(TokenType::OP_EQ); return; }
                    add_error("unexpected '=' (did you mean '=='?)");
                    return;
                case '!':
                    emit(match('=') ? TokenType::OP_NE : TokenType::OP_LNOT);
                    return;
                case '&':
                    emit(match('&') ? TokenType::OP_LAND : TokenType::OP_BAND);
                    return;
                case '|':
                    emit(match('|') ? TokenType::OP_LOR : TokenType::OP_BOR);
                    return;

                // --- Dot family ---------------------------------------------
                // DDOT > DOT by maximal munch. A '.' followed by a digit starts
                // a leading-dot real literal (Spec 1.4 REAL_NUM = \.[0-9]+...).
                case '.':
                    if (match('.')) { emit(TokenType::DDOT); return; }
                    if (is_digit(peek()))
                    {
                        // Rewind so the numeric scanner sees the leading dot.
                        current_ = start_;
                        scan_numeric_literal();
                        return;
                    }
                    emit(TokenType::DOT);
                    return;

                // --- String literals (Spec 1.3) ------------------------------
                case '"':
                    scan_string_literal();
                    return;
                case '\'':
                    if (match('\'')) { scan_raw_string_literal(); return; }
                    // Single quote on its own is not part of any production.
                    add_error("stray '\'' in input");
                    return;

                default:
                    add_error("unexpected character");
                    return;
            }
        }

        // ---------------------------------------------------------------------
        // Identifier / keyword
        // ---------------------------------------------------------------------

        void Scanner::scan_identifier_or_keyword()
        {
            // Spec 1.1: IDENTIFIER = [A-Za-z_][A-Za-z0-9_]*
            advance();  // first alpha; caller guaranteed via is_alpha(peek()).
            while (!is_at_end() && is_alnum(peek())) advance();

            std::string lexeme(source_.data() + start_, current_ - start_);
            const auto& table = keyword_table();
            auto it = table.find(lexeme);
            emit(it != table.end() ? it->second : TokenType::IDENTIFIER);
        }

        // ---------------------------------------------------------------------
        // String literals
        // ---------------------------------------------------------------------

        void Scanner::scan_string_literal()
        {
            // Opening '"' has already been consumed by scan_one_token.
            // Body alphabet per Spec 1.3:
            //   \[nrfbt"\\]  |  \x HH  |  \0 OOO  |  [^"\\]
            while (!is_at_end())
            {
                char c = peek();
                if (c == '"')
                {
                    advance();
                    emit(TokenType::STRING_LITERAL);
                    return;
                }
                if (c == '\\')
                {
                    advance();
                    char esc = peek();
                    switch (esc)
                    {
                        case 'n': case 'r': case 'f': case 'b': case 't':
                        case '"': case '\\':
                            advance();
                            break;
                        case 'x':
                            advance();
                            if (is_hex_digit(peek()) && is_hex_digit(peek(1)))
                            {
                                advance(); advance();
                            }
                            else
                            {
                                add_error("invalid hex escape in string");
                                continue;
                            }
                            break;
                        case '0':
                            advance();
                            if (is_octal_digit(peek()) &&
                                is_octal_digit(peek(1)) &&
                                is_octal_digit(peek(2)))
                            {
                                advance(); advance(); advance();
                            }
                            else
                            {
                                add_error("invalid octal escape in string");
                                continue;
                            }
                            break;
                        default:
                            advance();
                            add_error("invalid escape sequence in string");
                            continue;
                    }
                    continue;
                }
                // Any other character. Spec's [^"\\] does not exclude newlines,
                // but the driver normally feeds one line at a time so this is
                // only exercised for the raw-string path.
                if (c == '\n')
                {
                    line_++;
                    line_start_ = current_ + 1;
                }
                advance();
            }
            // Reached EOF without a closing quote.
            add_error("unterminated string");
        }

        void Scanner::scan_raw_string_literal()
        {
            // Opening "''" has already been consumed. The body matches the
            // lazy regex [\s\S]*? terminated by another "''" (Spec 1.3).
            while (!is_at_end())
            {
                if (peek() == '\'' && peek(1) == '\'')
                {
                    advance();
                    advance();
                    emit(TokenType::RAW_STRING_LITERAL);
                    return;
                }
                char c = peek();
                if (c == '\n')
                {
                    line_++;
                    line_start_ = current_ + 1;
                }
                advance();
            }
            add_error("unterminated raw string");
        }

        // ---------------------------------------------------------------------
        // Numeric literals
        // ---------------------------------------------------------------------

        void Scanner::scan_numeric_literal()
        {
            // Spec 2.7: <numeric_literal> ::= <numeric_base> <numeric_suffix_opt>
            // NUMERIC_BASE and optionally NUMERIC_SUFFIX.
            scan_numeric_base();
            scan_numeric_suffix();
        }

        void Scanner::scan_numeric_base()
        {
            // Hex: 0[xX][0-9A-Fa-f]+ . Per Spec 1.7 rule 2, hex wins over the
            // octal/decimal forms when both could match, so we try it first.
            // Note: IMAG_NUM in the spec is decimal-or-real followed by 'i';
            // hex literals therefore CANNOT carry the imaginary 'i' suffix.
            if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X') &&
                is_hex_digit(peek(2)))
            {
                advance();
                advance();
                while (is_hex_digit(peek())) advance();
                emit(TokenType::NUMERIC_BASE);
                return;
            }

            // Decimal / real / imaginary along the same track.
            //
            // We consume an INT_DEC-shaped prefix, then optionally fractional
            // and exponent parts, then an optional trailing 'i' for imaginary.
            // The result is classified as NUMERIC_BASE regardless of which
            // alternative actually matched - the parser inspects the lexeme.
            if (peek() == '.')
            {
                // Leading-dot real, e.g. ".5" or ".25e6".
                advance();
                while (is_digit(peek())) advance();
            }
            else
            {
                while (is_digit(peek())) advance();
                // Fractional part: ONLY consume '.' if a digit follows. This
                // deviates from the spec REAL_NUM regex (which permits "5.")
                // so that "5..7" tokenises as 5, .., 7 instead of "5.", ".",
                // "7". The pragmatic trade-off keeps DDOT working naturally.
                if (peek() == '.' && is_digit(peek(1)))
                {
                    advance();
                    while (is_digit(peek())) advance();
                }
            }

            // Exponent: [eE][+-]?[0-9]+. We require at least one digit after
            // the (optional) sign; otherwise we roll back so "1e" stays as
            // NUMERIC_BASE("1") followed by IDENTIFIER("e").
            if (peek() == 'e' || peek() == 'E')
            {
                std::size_t save = current_;
                advance();
                if (peek() == '+' || peek() == '-') advance();
                if (is_digit(peek()))
                {
                    while (is_digit(peek())) advance();
                }
                else
                {
                    current_ = save;
                }
            }

            // Imaginary suffix.
            if (peek() == 'i') advance();

            emit(TokenType::NUMERIC_BASE);
        }

        void Scanner::scan_numeric_suffix()
        {
            // Spec 1.7 rule 3: longest valid suffix emitted as a single
            // NUMERIC_SUFFIX token.
            // Priority: PREDEF_SCALED_UNIT > SCALE_FACTOR + UNIT > UNIT.

            // Optional whitespace between NUMERIC_BASE and suffix ("1 Hz").
            while (!is_at_end() && (peek() == ' ' || peek() == '\t')) advance();

            const char* rest = source_.data() + current_;
            const std::size_t rest_size = source_.size() - current_;

            const std::size_t predef_len = match_predef_scaled_unit(rest, rest_size);

            std::size_t sf_len = 0;
            std::size_t sf_unit_len = 0;
            if (rest_size > 0 && is_scale_factor_char(rest[0]))
            {
                sf_len = 1;
                sf_unit_len = match_unit(rest + 1, rest_size - 1);
            }
            const std::size_t form2_len = sf_len + sf_unit_len;

            const std::size_t unit_len = match_unit(rest, rest_size);

            // Pick the longest match.
            std::size_t best_len = predef_len;
            if (form2_len > best_len) best_len = form2_len;
            if (unit_len > best_len)  best_len = unit_len;

            // Right-boundary guard: the suffix must not be followed by an
            // alphanumeric character, otherwise it's part of an identifier
            // or keyword (e.g. "1AND1" → 1, AND, 1, not 1A, ND1;
            // "8ms" → 8, ms instead of 8, m, s).
            if (best_len > 0 && is_alnum(peek(best_len)))
            {
                best_len = 0;
            }

            if (best_len > 0)
            {
                mark_token_start();
                for (std::size_t i = 0; i < best_len; ++i) advance();
                emit(TokenType::NUMERIC_SUFFIX);
                return;
            }
            // No suffix.
        }
} // namespace rel
