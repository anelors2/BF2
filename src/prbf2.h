#pragma once
#include <cstdint>
#include "memory.h"

#include "prbf2_offsets.h"

struct GameState {
    uint32_t team1Tickets = 0;
    uint32_t team2Tickets = 0;
    uint8_t winner = 0;
    uint8_t victoryType = 0;
    uintptr_t managerPtr = 0;
    bool inRound = false;
};

class PRBF2Interface {
public:
    Memory mem;
    GameState state;

    bool freezeTeam1 = false;
    uint32_t freezeTeam1Value = 9999;
    bool freezeTeam2 = false;
    uint32_t freezeTeam2Value = 0;

    bool TryConnect() {
        if (!mem.IsAttached()) {
            return mem.Attach(L"PRBF2.exe");
        }
        return true;
    }

    uintptr_t GetTeamTicketsAddress(int teamId) {
        if (!state.managerPtr) return 0;
        return state.managerPtr + prbf2::game_logic::Tickets(teamId);
    }

    void Update() {
        if (!TryConnect()) {
            state.inRound = false;
            state.managerPtr = 0;
            return;
        }

        state.managerPtr = mem.Read<uint32_t>(prbf2::exe::kGameLogic);

        if (!state.managerPtr) {
            state.inRound = false;
            return;
        }

        state.inRound = true;

        uintptr_t addrTeam1 = GetTeamTicketsAddress(1);
        uintptr_t addrTeam2 = GetTeamTicketsAddress(2);

        if (freezeTeam1 && addrTeam1) {
            mem.Write<uint32_t>(addrTeam1, freezeTeam1Value);
        }
        if (freezeTeam2 && addrTeam2) {
            mem.Write<uint32_t>(addrTeam2, freezeTeam2Value);
        }

        state.team1Tickets = mem.Read<uint32_t>(addrTeam1);
        state.team2Tickets = mem.Read<uint32_t>(addrTeam2);
        state.winner = mem.Read<uint8_t>(state.managerPtr + prbf2::game_logic::kWinner);
        state.victoryType = mem.Read<uint8_t>(state.managerPtr + prbf2::game_logic::kVictoryType);
    }

    bool SetTickets(int teamId, uint32_t tickets) {
        uintptr_t addr = GetTeamTicketsAddress(teamId);
        if (!addr) return false;
        return mem.Write<uint32_t>(addr, tickets);
    }
};
