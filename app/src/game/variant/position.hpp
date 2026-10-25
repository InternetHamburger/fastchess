#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <chess.hpp>
#include <types/enums.hpp>
#include <types/exception.hpp>
#include <core/logger/logger.hpp>

namespace fastchess::rules {

struct AtomicTurn {
    int from = -1;
    int to = -1;
    char promotion = 0;
};

class AtomicPosition {
   public:
    explicit AtomicPosition();
    bool setFen(std::string_view fen);
    bool setEpd(std::string_view epd);
    std::string getFen(bool counters = true) const;
    std::string getEpd() const;
    chess::Color sideToMove() const { return white_ ? chess::Color::WHITE : chess::Color::BLACK; }
    int fullMoveNumber() const { return fullmove_; }
    int halfMoveClock() const { return halfmove_; }
    static std::optional<AtomicTurn> parseTurn(std::string_view move);
    static std::string toUci(const AtomicTurn& move);
    bool isLegal(const AtomicTurn& move) const;
    bool isLegal(std::string_view move) const;
    void play(std::string_view move);
    std::string notation(std::string_view move, NotationType type) const;
    std::string parseNotation(std::string_view move) const;
    std::pair<chess::GameResultReason, chess::GameResult> isGameOver() const;

   private:
    int castlingIndex(const AtomicTurn& move) const;
    bool pieceLegal(const AtomicTurn& move) const;
    void explode(int sq);
    void applyPiece(const AtomicTurn& move);
    bool kingCaptured(const AtomicTurn& move, const chess::Bitboard& enemy_occ) const;
    bool hasMove() const;
    std::string repetitionKey() const;
    std::string san(const AtomicTurn& move, bool long_notation) const;
    std::array<char, 64> squares_{};
    bool white_ = true;
    int ep_ = -1;
    int rights_ = 0;
    std::array<int, 4> rooks_{7, 0, 63, 56};
    bool chess960_ = false;
    int halfmove_ = 0;
    int fullmove_ = 1;
    std::vector<std::string> history_;
};

class Position {
   public:
    explicit Position(VariantType variant = VariantType::STANDARD) : variant_(variant) {
        if (variant == VariantType::FRC) {
            throw fastchess_exception("Atomic does not support fischer random");
        }
        chess_.set960(false);
        if (isAtomic()) atomic_.emplace();
    }
    bool isAtomic() const { return variant_ == VariantType::ATOMIC; }
    bool setFen(std::string_view fen) { return isAtomic() ? atomic_->setFen(fen) : chess_.setFen(fen); }
    bool setEpd(std::string_view epd) { return isAtomic() ? atomic_->setEpd(epd) : chess_.setEpd(epd); }
    std::string getFen() const { return isAtomic() ? atomic_->getFen() : chess_.getFen(); }
    std::string getEpd() const { return isAtomic() ? atomic_->getEpd() : chess_.getEpd(); }
    chess::Color sideToMove() const { return isAtomic() ? atomic_->sideToMove() : chess_.sideToMove(); }
    int fullMoveNumber() const { return isAtomic() ? atomic_->fullMoveNumber() : chess_.fullMoveNumber(); }
    int halfMoveClock() const { return isAtomic() ? atomic_->halfMoveClock() : chess_.halfMoveClock(); }
    auto isGameOver() const { return isAtomic() ? atomic_->isGameOver() : chess_.isGameOver(); }
    const chess::Board& orthodox() const { return chess_; }
    chess::Board& orthodox() { return chess_; }
    bool isLegal(std::string_view move) const;
    void play(std::string_view move);
    std::string notation(std::string_view move, NotationType type) const;
    std::string parseNotation(std::string_view move) const;

   private:
    VariantType variant_;
    chess::Board chess_;
    std::optional<AtomicPosition> atomic_;
};

}