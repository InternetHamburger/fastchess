#include "position.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>

#include <chess.hpp>
#include <types/enums.hpp>

#include "../../../third_party/chess.hpp"
#include "../../types/enums.hpp"

namespace fastchess::rules {
namespace {
int square(std::string_view s) {
    if (s.size() != 2 || s[0] < 'a' || s[0] > 'h' || s[1] < '1' || s[1] > '8') return -1;
    return (s[1] - '1') * 8 + s[0] - 'a';
}
std::string squareName(int s) { return {char('a' + s % 8), char('1' + s / 8)}; }
char lower(char c) { return char(std::tolower(static_cast<unsigned char>(c))); }
bool whitePiece(char c) { return c >= 'A' && c <= 'Z'; }
bool number(std::string_view s, int& value, int minimum) {
    if (s.empty()) return false;
    int result = 0;
    for (char c : s) {
        if (c < '0' || c > '9' || result > (std::numeric_limits<int>::max() - (c - '0')) / 10) return false;
        result = result * 10 + c - '0';
    }
    if (result < minimum || result > 1000000000) return false;
    value = result;
    return true;
}
}

AtomicPosition::AtomicPosition() { setFen(chess::constants::STARTPOS); }

bool AtomicPosition::setFen(std::string_view fen) {
    std::istringstream input{std::string(fen)};
    std::vector<std::string> fields;
    for (std::string s; input >> s;) fields.push_back(s);
    if (fields.size() != 4 && fields.size() != 6) return false;
    std::array<char, 64> board{};
    int rank = 7, file = 0, white_kings = 0, black_kings = 0;
    for (char c : fields[0]) {
        if (c == '/') {
            if (file != 8 || rank == 0) return false;
            --rank;
            file = 0;
        } else if (c >= '1' && c <= '8') {
            file += c - '0';
            if (file > 8) return false;
        } else {
            if (file >= 8 || std::string_view("PNBRQKpnbrqk").find(c) == std::string_view::npos) return false;
            const int sq = rank * 8 + file++;
            if (lower(c) == 'p' && (rank == 0 || rank == 7)) return false;
            board[sq] = c;
            white_kings += c == 'K';
            black_kings += c == 'k';
        }
    }
    if (rank != 0 || file != 8 || white_kings > 1 || black_kings > 1 || white_kings + black_kings == 0) return false;
    if (fields[1] != "w" && fields[1] != "b") return false;
    int rights = 0;
    std::array<int, 4> rooks{7, 0, 63, 56};
    if (fields[2] != "-") {
        if (fields[2].empty()) return false;
        for (char c : fields[2]) {
            int bit = -1;
            if (chess960_) {
                const bool white = whitePiece(c);
                const int base = white ? 0 : 56;
                const auto king = std::find(board.begin() + base, board.begin() + base + 8, white ? 'K' : 'k');
                if (king == board.begin() + base + 8) return false;
                const int king_file = int(king - board.begin()) % 8;
                int rook_file = -1;
                if (lower(c) == 'k') {
                    for (int f = 7; f > king_file; --f) if (board[base + f] == (white ? 'R' : 'r')) { rook_file = f; break; }
                } else if (lower(c) == 'q') {
                    for (int f = 0; f < king_file; ++f) if (board[base + f] == (white ? 'R' : 'r')) { rook_file = f; break; }
                } else if (lower(c) >= 'a' && lower(c) <= 'h') rook_file = lower(c) - 'a';
                if (rook_file < 0 || rook_file == king_file || board[base + rook_file] != (white ? 'R' : 'r')) return false;
                bit = (white ? 0 : 2) + (rook_file < king_file);
                rooks[bit] = base + rook_file;
            } else {
                const auto index = std::string_view("KQkq").find(c);
                if (index == std::string_view::npos) return false;
                bit = int(index);
                if (board[bit < 2 ? 4 : 60] != (bit < 2 ? 'K' : 'k') || board[rooks[bit]] != (bit < 2 ? 'R' : 'r')) return false;
            }
            if (rights & (1 << bit)) return false;
            rights |= 1 << bit;
        }
    }
    const bool white = fields[1] == "w";
    int ep = -1, halfmove = 0, fullmove = 1;
    if (fields[3] != "-") {
        ep = square(fields[3]);
        if (ep < 0 || ep / 8 != (white ? 5 : 2) || board[ep] ||
            board[ep + (white ? -8 : 8)] != (white ? 'p' : 'P') || board[ep + (white ? 8 : -8)]) return false;
    }
    if (fields.size() == 6 && (!number(fields[4], halfmove, 0) || !number(fields[5], fullmove, 1))) return false;
    squares_ = board;
    white_ = white;
    rights_ = rights;
    rooks_ = rooks;
    ep_ = ep;
    halfmove_ = halfmove;
    fullmove_ = fullmove;
    history_.clear();
    history_.push_back(repetitionKey());
    return true;
}

bool AtomicPosition::setEpd(std::string_view epd) {
    std::istringstream input{std::string(epd)};
    std::string placement, side, rights, ep;
    if (!(input >> placement >> side >> rights >> ep)) return false;
    int halfmove = 0, fullmove = 1;
    std::string operation;
    while (std::getline(input, operation, ';')) {
        std::istringstream op(operation);
        std::string key, value, extra;
        op >> key;
        if (key == "hmvc" || key == "fmvn") {
            if (!(op >> value) || (op >> extra)) return false;
            if (!number(value, key == "hmvc" ? halfmove : fullmove, key == "hmvc" ? 0 : 1)) return false;
        }
    }
    return setFen(placement + " " + side + " " + rights + " " + ep + " " + std::to_string(halfmove) + " " + std::to_string(fullmove));
}

std::string AtomicPosition::getFen(bool counters) const {
    std::string fen;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            const int sq = rank * 8 + file;
            const char piece = squares_[sq];
            if (!piece) { ++empty; continue; }
            if (empty) { fen += char('0' + empty); empty = 0; }
            fen += piece;
        }
        if (empty) fen += char('0' + empty);
        if (rank) fen += '/';
    }
    fen += white_ ? " w " : " b ";
    if (!rights_) fen += '-';
    for (int i = 0; i < 4; ++i) if (rights_ & (1 << i)) fen += chess960_ ? char((i < 2 ? 'A' : 'a') + rooks_[i] % 8) : "KQkq"[i];
    fen += " " + (ep_ == -1 ? std::string("-") : squareName(ep_));
    if (counters) fen += " " + std::to_string(halfmove_) + " " + std::to_string(fullmove_);
    return fen;
}

std::string AtomicPosition::getEpd() const {
    return getFen(false) + " hmvc " + std::to_string(halfmove_) + "; fmvn " + std::to_string(fullmove_) + ";";
}

std::optional<AtomicTurn> AtomicPosition::parseTurn(std::string_view move) {
    if (move.size() != 4 && move.size() != 5) return std::nullopt;
    AtomicTurn turn;
    turn.from = square(move.substr(0, 2));
    turn.to = square(move.substr(2, 2));
    if (turn.from < 0 || turn.to < 0 || turn.from == turn.to) return std::nullopt;
    if (move.size() == 5) {
        if (std::string_view("nbrq").find(move[4]) == std::string_view::npos) return std::nullopt;
        turn.promotion = move[4];
    }
    return turn;
}

std::string AtomicPosition::toUci(const AtomicTurn& move) {
    std::string result = squareName(move.from) + squareName(move.to);
    if (move.promotion) result += move.promotion;
    return result;
}

int AtomicPosition::castlingIndex(const AtomicTurn& move) const {
    if (move.from < 0 || move.from >= 64 || squares_[move.from] != (white_ ? 'K' : 'k')) return -1;
    const int base = white_ ? 0 : 56;
    if (move.from / 8 != base / 8) return -1;
    for (int i = white_ ? 0 : 2; i < (white_ ? 2 : 4); ++i) {
        if ((rights_ & (1 << i)) && move.to == (chess960_ ? rooks_[i] : base + (i % 2 ? 2 : 6))) return i;
    }
    return -1;
}

bool AtomicPosition::pieceLegal(const AtomicTurn& move) const {
    const int from = move.from, to = move.to;
    if (from < 0 || from >= 64 || to < 0 || to >= 64 || from == to) return false;
    const char p = squares_[from], target = squares_[to];
    const int castle = castlingIndex(move);
    if (castle >= 0) {
        if (move.promotion || squares_[rooks_[castle]] != (white_ ? 'R' : 'r')) return false;
        const int base = white_ ? 0 : 56;
        const int king_to = base + (castle % 2 ? 2 : 6), rook_to = base + (castle % 2 ? 3 : 5);
        const auto clear = [&](int start, int end) {
            for (int sq = std::min(start, end); sq <= std::max(start, end); ++sq)
                if (sq != from && sq != rooks_[castle] && squares_[sq]) return false;
            return true;
        };
        return clear(from, king_to) && clear(rooks_[castle], rook_to);
    }
    if (!p || whitePiece(p) != white_ || (target && whitePiece(target) == white_)) return false;
    const char type = lower(p);
    const bool promotion = type == 'p' && to / 8 == (white_ ? 7 : 0);
    if (promotion != bool(move.promotion) || (promotion && std::string_view("nbrq").find(move.promotion) == std::string_view::npos)) return false;
    const int dx = to % 8 - from % 8, dy = to / 8 - from / 8;
    const int ax = std::abs(dx), ay = std::abs(dy);
    if (type == 'p') {
        const int dir = white_ ? 1 : -1;
        if (ax == 1 && dy == dir) return target || (to == ep_ && squares_[to - 8 * dir] == (white_ ? 'p' : 'P'));
        if (dx || target) return false;
        if (dy == dir) return true;
        return dy == 2 * dir && from / 8 == (white_ ? 1 : 6) && !squares_[from + 8 * dir];
    }
    if (type == 'n') return ax * ay == 2;
    if (type == 'k') return std::max(ax, ay) == 1;
    const bool diagonal = ax == ay;
    const bool straight = dx == 0 || dy == 0;
    if ((type == 'b' && !diagonal) || (type == 'r' && !straight) || (type == 'q' && !diagonal && !straight)) return false;
    const int step = (dx > 0 ? 1 : dx < 0 ? -1 : 0) + 8 * (dy > 0 ? 1 : dy < 0 ? -1 : 0);
    for (int sq = from + step; sq != to; sq += step) if (squares_[sq]) return false;
    return true;
}

bool AtomicPosition::kingCaptured(const AtomicTurn& move, const chess::Bitboard& enemy_occ) const {
    return lower(squares_[move.to]) == 'k' || chess::attacks::king(move.to) & enemy_occ;
}

void AtomicPosition::explode(int sq) {
    chess::Bitboard exploded_bb = chess::attacks::king(sq);
    squares_[sq] = 0;
    while (exploded_bb.getBits() != 0) {
        sq = exploded_bb.pop();
        if (tolower(squares_[sq]) != 'p')
            squares_[sq] = 0;
    }
}

void AtomicPosition::applyPiece(const AtomicTurn& move) {
    const int castle = castlingIndex(move);
    if (castle >= 0) {
        const int base = white_ ? 0 : 56;
        squares_[move.from] = 0;
        squares_[rooks_[castle]] = 0;
        squares_[base + (castle % 2 ? 2 : 6)] = white_ ? 'K' : 'k';
        squares_[base + (castle % 2 ? 3 : 5)] = white_ ? 'R' : 'r';
        rights_ &= white_ ? ~3 : ~12;
        ep_ = -1;
        ++halfmove_;
        return;
    }
    const char piece = squares_[move.from], target = squares_[move.to];
    const bool pawn = lower(piece) == 'p';
    const int dir = white_ ? 8 : -8;
    if (pawn && move.to == ep_ && !target && move.from % 8 != move.to % 8) {
        squares_[move.to - dir] = 0;
        explode(move.to);
    }
    if (target) {
        explode(move.to);

    }
    if (lower(piece) == 'k') {
        rights_ &= white_ ? ~3 : ~12;
    }
    for (int i = 0; i < 4; ++i) {
        const int rook = rooks_[i];
        if (move.from == rook || move.to == rook || (target && chess::attacks::king(move.to) & 1ull << move.to)) rights_ &= ~(1 << i);
    }
    if (lower(target) == 'k') rights_ &= white_ ? ~12 : ~3;
    squares_[move.from] = 0;
    squares_[move.to] = move.promotion ? (white_ ? char(std::toupper(move.promotion)) : move.promotion) : piece;
    ep_ = pawn && std::abs(move.to - move.from) == 16 ? (move.to + move.from) / 2 : -1;
    halfmove_ = pawn || target ? 0 : halfmove_ + 1;
}

bool AtomicPosition::isLegal(const AtomicTurn& move) const {
    if (!pieceLegal(move)) return false;
    if (std::find(squares_.begin(), squares_.end(), 'K') == squares_.end() ||
        std::find(squares_.begin(), squares_.end(), 'k') == squares_.end()) return false;

    if (lower(squares_[move.from]) == 'p' && move.to == ep_ && !squares_[move.to] &&
        move.from % 8 != move.to % 8) return true;
    return true;
}

bool AtomicPosition::isLegal(std::string_view move) const {
    const auto parsed = parseTurn(move);
    return parsed && isLegal(*parsed);
}

void AtomicPosition::play(std::string_view move) {
    const auto parsed = parseTurn(move);
    if (!parsed || !isLegal(*parsed)) throw std::invalid_argument("Illegal Atomic Chess turn: " + std::string(move));
    applyPiece(*parsed);
    fullmove_ += !white_;
    white_ = !white_;
    history_.push_back(repetitionKey());
}

std::string AtomicPosition::repetitionKey() const {
    auto fen = getFen(false);
    bool possible_ep = false;
    if (ep_ >= 0) {
        for (int from = 0; from < 64; ++from) {
            if (lower(squares_[from]) == 'p' && pieceLegal({from, ep_, -1})) { possible_ep = true; break; }
        }
    }
    if (!possible_ep) fen.replace(fen.find_last_of(' ') + 1, std::string::npos, "-");
    return fen;
}

bool AtomicPosition::hasMove() const {
    for (int from = 0; from < 64; ++from) {
        if (!squares_[from] || whitePiece(squares_[from]) != white_) continue;
        for (int to = 0; to < 64; ++to) {
            AtomicTurn move{from, to, 0};
            if (lower(squares_[from]) == 'p' && to / 8 == (white_ ? 7 : 0)) move.promotion = 'q';
            if (pieceLegal(move)) return true;
        }
    }
    return false;
}

std::pair<chess::GameResultReason, chess::GameResult> AtomicPosition::isGameOver() const {
    using Reason = chess::GameResultReason;
    using Result = chess::GameResult;
    if (std::find(squares_.begin(), squares_.end(), white_ ? 'K' : 'k') == squares_.end()) return {Reason::CHECKMATE, Result::LOSE};
    if (std::find(squares_.begin(), squares_.end(), white_ ? 'k' : 'K') == squares_.end()) return {Reason::CHECKMATE, Result::WIN};
    if (!hasMove()) return {Reason::STALEMATE, Result::WIN};
    if (halfmove_ >= 100) return {Reason::FIFTY_MOVE_RULE, Result::DRAW};
    if (std::count(history_.begin(), history_.end(), repetitionKey()) >= 3) return {Reason::THREEFOLD_REPETITION, Result::DRAW};
    return {Reason::NONE, Result::NONE};
}

std::string AtomicPosition::san(const AtomicTurn& move, bool long_notation) const {
    const char type = lower(squares_[move.from]);
    std::string result;
    const int castle = castlingIndex(move);
    if (castle >= 0) {
        result = castle % 2 ? "O-O-O" : "O-O";
    } else {
        const bool capture = squares_[move.to] || (type == 'p' && move.from % 8 != move.to % 8);
        if (type != 'p') result += char(std::toupper(type));
        if (long_notation) result += squareName(move.from);
        else if (type == 'p') { if (capture) result += char('a' + move.from % 8); }
        else {
            bool ambiguous = false, same_file = false, same_rank = false;
            for (int from = 0; from < 64; ++from) {
                if (from == move.from || squares_[from] != squares_[move.from]) continue;
                auto other = move;
                other.from = from;
                if (!pieceLegal(other)) continue;
                ambiguous = true;
                same_file |= from % 8 == move.from % 8;
                same_rank |= from / 8 == move.from / 8;
            }
            if (ambiguous) {
                if (!same_file || same_rank) result += char('a' + move.from % 8);
                if (same_file) result += char('1' + move.from / 8);
            }
        }
        if (capture) result += 'x';
        else if (long_notation) result += '-';
        result += squareName(move.to);
        if (move.promotion) { result += '='; result += char(std::toupper(move.promotion)); }
    }
    return result;
}

std::string AtomicPosition::notation(std::string_view move, NotationType type) const {
    const auto parsed = parseTurn(move);
    if (!parsed || !isLegal(*parsed)) throw std::invalid_argument("Invalid Atomic Chess move notation");
    return type == NotationType::UCI ? toUci(*parsed) : san(*parsed, type == NotationType::LAN);
}

std::string AtomicPosition::parseNotation(std::string_view text) const {
    std::string move(text);
    while (!move.empty() && (move.back() == '!' || move.back() == '?')) move.pop_back();
    if (isLegal(move)) return move;
    std::replace(move.begin(), move.end(), '0', 'O');

    std::optional<AtomicTurn> found;
    for (int from = 0; from < 64; ++from) {
        if (!squares_[from] || whitePiece(squares_[from]) != white_) continue;
        for (int to = 0; to < 64; ++to) {
            const bool promotion = lower(squares_[from]) == 'p' && to / 8 == (white_ ? 7 : 0);
            for (char p : promotion ? std::string("nbrq") : std::string(1, '\0')) {
                AtomicTurn turn{from, to, p};
                if (!isLegal(turn)) continue;
                if (san(turn, false) != move && san(turn, true) != move) continue;
                if (found) throw std::invalid_argument("Ambiguous Atomic Chess PGN move");
                found = turn;
            }
        }
    }
    if (!found) throw std::invalid_argument("Invalid Atomic Chess PGN move: " + move);
    return toUci(*found);
}

bool Position::isLegal(std::string_view move) const {
    if (isAtomic()) return atomic_->isLegal(move);
    if (!chess::uci::isUciMove(move)) return false;
    const auto parsed = chess::uci::uciToMove(chess_, move);
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, chess_);
    return std::find(moves.begin(), moves.end(), parsed) != moves.end();
}

void Position::play(std::string_view move) {
    if (isAtomic()) atomic_->play(move);
    else chess_.makeMove<true>(chess::uci::uciToMove(chess_, move));
}

std::string Position::notation(std::string_view move, NotationType type) const {
    if (isAtomic()) return atomic_->notation(move, type);
    const auto parsed = chess::uci::uciToMove(chess_, move);
    if (type == NotationType::SAN) return chess::uci::moveToSan(chess_, parsed);
    if (type == NotationType::LAN) return chess::uci::moveToLan(chess_, parsed);
    return std::string(move);
}

std::string Position::parseNotation(std::string_view move) const {
    if (isAtomic()) return atomic_->parseNotation(move);
    return chess::uci::moveToUci(chess::uci::parseSan(chess_, move), chess_.chess960());
}

}