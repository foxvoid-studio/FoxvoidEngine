#pragma once

#include "ICommand.hpp"
#include "graphics/renderers/TileMap.hpp"
#include <vector>

class TileMapPaintCommand : public ICommand {
    public:
        TileMapPaintCommand(TileMap* tileMap, int layerIndex, const std::vector<int>& oldData, const std::vector<int>& newData);

        // Executes the action (or re-executes it on Redo)
        void Execute() override;
        
        // Reverts the action
        void Undo() override;

    private:
        TileMap* m_tileMap;
        int m_layerIndex;
        
        // We store the full grid state before and after the paint stroke
        std::vector<int> m_oldData;
        std::vector<int> m_newData;
};
