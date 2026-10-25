#pragma once

#include <string>

#include <chess.hpp>

#include <core/config/config.hpp>
#include <matchmaking/match/match.hpp>
#include <types/tournament.hpp>

#include <fmt/include/fmt/format.h>

namespace fastchess::epd {

class EpdBuilder {
   public:
    EpdBuilder(const VariantType& variant, const MatchData& match) {
        rules::Position board(variant);
        board.setFen(match.fen);

        for (const auto& move : match.moves) {
            const auto illegal = !move.legal;

            if (illegal) break;

            board.play(move.move);
        }

        epd = fmt::format("{}\n", board.getEpd());
    }

    // Get the newly created epd
    [[nodiscard]] std::string get() const noexcept { return epd; }

   private:
    std::string epd;
};

}  // namespace fastchess::epd
