#include "SceneViewPanel.hpp"
#include <rlImGui.h>
#include "ImGuizmo.h"
#include <raymath.h>
#include "extras/IconsFontAwesome6.h"
#include "editor/commands/CommandHistory.hpp"
#include "editor/commands/Transform2dCommand.hpp"
#include "graphics/renderers/ShapeRenderer.hpp"
#include "graphics/renderers/TileMap.hpp"
#include "editor/commands/TileMapPaintCommand.hpp"
#include "graphics/renderers/SpriteRenderer.hpp"
#include "graphics/renderers/SpriteSheetRenderer.hpp"

// Note the & on selectedTileID, selectedLayer, and currentTileTool
void SceneViewPanel::Draw(RenderTexture2D& sceneTexture, EditorCamera& camera, Scene& activeScene, GameObject*& selectedObject, int& selectedTileID, int& selectedLayer, TileTool& currentTileTool, EditorViewMode& currentViewMode) {    
    // Remove inner margins (padding) so the render texture touches the window borders
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Scene View");

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        currentViewMode = EditorViewMode::Scene;
    }

    // Gizmo operation state
    // We store the current tool (Translate, Rotate, Scale)
    static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_W)) currentGizmoOperation = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_E)) currentGizmoOperation = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) currentGizmoOperation = ImGuizmo::SCALE;
    }

    // Check if the user's mouse is currently hovering this specific ImGui window
    bool isHovered = ImGui::IsWindowHovered();

    // Don't move the camera if we are currently dragging the Gizmo
    if (!ImGuizmo::IsUsing()) {
        // Pass the hover state to the camera so it only pans/zooms when appropriate
        camera.Update(isHovered);
    }

    // Get the available size inside this specific ImGui window
    ImVec2 windowSize = ImGui::GetContentRegionAvail();
    if (windowSize.x > 0.0f && windowSize.y > 0.0f) {
        // Aspect Ratio Calculation
        float texWidth = (float)sceneTexture.texture.width;
        float texHeight = (float)sceneTexture.texture.height;
        float targetAspect = texWidth / texHeight;
        float windowAspect = windowSize.x / windowSize.y;

        ImVec2 drawSize;
        if (windowAspect > targetAspect) {
            // Window is wider than the texture -> Fit to height
            drawSize.y = windowSize.y;
            drawSize.x = windowSize.y * targetAspect;
        } else {
            // Window is taller than the texture -> Fit to width
            drawSize.x = windowSize.x;
            drawSize.y = windowSize.x / targetAspect;
        }

        // Calculate centered position
        ImVec2 cursorPos = ImGui::GetCursorPos(); // Top-left of the available region
        cursorPos.x += (windowSize.x - drawSize.x) * 0.5f;
        cursorPos.y += (windowSize.y - drawSize.y) * 0.5f;
        ImGui::SetCursorPos(cursorPos);

        ImVec2 imagePosAbsolute = ImGui::GetCursorScreenPos();

        // Draw the image with the correctly scaled size
        Rectangle sourceRec = { 0.0f, 0.0f, texWidth, -texHeight };
        rlImGuiImageRect(&sceneTexture.texture, (int)drawSize.x, (int)drawSize.y, sourceRec);

        // We must capture if the image is hovered right here before drawing anything else on top of it 
        bool isImageHovered = ImGui::IsItemHovered();

        if (selectedObject) {
            auto transform = selectedObject->GetComponent<Transform2d>();
            if (transform) {
                // Calculate world bounds (fallback size 50x50)
                float width = 50.0f * std::abs(transform->scale.x);
                float height = 50.0f * std::abs(transform->scale.y);

                if (auto sprite = selectedObject->GetComponent<SpriteRenderer>(); sprite && sprite->GetTexture().id != 0) {
                    width = sprite->GetTexture().width * std::abs(transform->scale.x);
                    height = sprite->GetTexture().height * std::abs(transform->scale.y);
                }
                else if (auto spriteSheet = selectedObject->GetComponent<SpriteSheetRenderer>(); spriteSheet && spriteSheet->GetTexture().id != 0) {
                    width = spriteSheet->GetSourceRec().width * std::abs(transform->scale.x);
                    height = spriteSheet->GetSourceRec().height * std::abs(transform->scale.y);
                }
                else if (auto shape = selectedObject->GetComponent<ShapeRenderer>()) {
                    width = shape->width * std::abs(transform->scale.x);
                    height = shape->height * std::abs(transform->scale.y);
                }

                // Define corners in World Space (assuming origin is center)
                auto position = transform->GetGlobalPosition();
                Vector2 topLeftWorld = { position.x - (width / 2.0f), position.y - (height / 2.0f) };
                Vector2 bottomRightWorld = { position.x + (width / 2.0f), position.y + (height / 2.0f) };

                // Convert from World Space to Render Texture Space
                Camera2D cam2d = camera.GetCamera();
                Vector2 topLeftTex = GetWorldToScreen2D(topLeftWorld, cam2d);
                Vector2 bottomRightTex = GetWorldToScreen2D(bottomRightWorld, cam2d);

                // Convert to ImGui Absolute Screen Space
                ImVec2 p_min = ImVec2(
                    imagePosAbsolute.x + (topLeftTex.x / texWidth) * drawSize.x,
                    imagePosAbsolute.y + (topLeftTex.y / texHeight) * drawSize.y
                );
                ImVec2 p_max = ImVec2(
                    imagePosAbsolute.x + (bottomRightTex.x / texWidth) * drawSize.x,
                    imagePosAbsolute.y + (bottomRightTex.y / texHeight) * drawSize.y
                );

                // Draw the rectangle
                ImGui::GetWindowDrawList()->AddRect(p_min, p_max, IM_COL32(255, 255, 255, 255), 0.0f, 0, 2.0f); 
            }
        }

        // Floating gizmo toolbar
        // Save cursor position to draw the toolbar at the top left of the scene view
        ImVec2 toolbarPos = ImVec2(10.0f, ImGui::GetCursorStartPos().y + 10.0f);
        ImGui::SetCursorPos(toolbarPos);

        // Styling the floating background
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 0.85f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);

        if (ImGui::BeginChild("GizmoToolbar", ImVec2(146, 48), false, ImGuiWindowFlags_NoScrollbar)) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::SetCursorPos(ImVec2(4, 4)); // Small internal padding

            // Scale up the font size specifically for this floating window (makes icons bigger)
            ImGui::SetWindowFontScale(1.3f);

            // Lambda helper to draw styled toggle buttons
            auto drawGizmoButton = [](const char* label, ImGuizmo::OPERATION op, ImGuizmo::OPERATION& current) {
                bool isSelected = (current == op);
                if (isSelected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.90f, 0.45f, 0.10f, 1.0f)); // Foxvoid Orange
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.00f, 0.55f, 0.20f, 1.0f));
                }
                
                if (ImGui::Button(label, ImVec2(40, 40))) current = op;
                
                if (isSelected) ImGui::PopStyleColor(2);
            };

            // Draw the 3 buttons with Icons
            drawGizmoButton(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, ImGuizmo::TRANSLATE, currentGizmoOperation);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Translate (W)");
            ImGui::SameLine();
            
            drawGizmoButton(ICON_FA_ROTATE, ImGuizmo::ROTATE, currentGizmoOperation);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rotate (E)");
            ImGui::SameLine();
            
            drawGizmoButton(ICON_FA_EXPAND, ImGuizmo::SCALE, currentGizmoOperation);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scale (R)");
            ImGui::PopStyleVar(2);
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        // ----------------------------------------------------
        // GIZMO VISIBILITY LOGIC
        // ----------------------------------------------------
        // When editing a TileMap, painting should take priority over the Transform Gizmo.
        // We hide the Gizmo by default for TileMaps, allowing you to paint anywhere.
        // Holding 'Left Control' temporarily reveals the Gizmo to let you move the map.
        bool isEditingTileMap = (selectedObject != nullptr && selectedObject->GetComponent<TileMap>() != nullptr);
        bool showGizmo = true;
        
        if (isEditingTileMap) {
            // Require Left Ctrl to show the Gizmo when a TileMap is selected
            showGizmo = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
        }

        // Gizmos
        if (selectedObject && showGizmo) {
            auto transform = selectedObject->GetComponent<Transform2d>();
            if (transform) {
                // Setup ImGuizmo environment
                ImGuizmo::SetOrthographic(true); // We are in 2D
                ImGuizmo::SetDrawlist();
                ImGuizmo::SetRect(imagePosAbsolute.x, imagePosAbsolute.y, drawSize.x, drawSize.y);

                // Extract Raylib camera matrices
                Camera2D cam2d = camera.GetCamera();
                Matrix viewMatrix = GetCameraMatrix2D(cam2d);
                Matrix projMatrix = MatrixOrtho(0.0f, texWidth, texHeight, 0.0f, -1.0f, 1.0f);

                // Raylib matrices are column-major, just like OpenGL/ImGuizmo expects.
                float view[16] = {
                    viewMatrix.m0, viewMatrix.m1, viewMatrix.m2, viewMatrix.m3,
                    viewMatrix.m4, viewMatrix.m5, viewMatrix.m6, viewMatrix.m7,
                    viewMatrix.m8, viewMatrix.m9, viewMatrix.m10, viewMatrix.m11,
                    viewMatrix.m12, viewMatrix.m13, viewMatrix.m14, viewMatrix.m15
                };
                
                float proj[16] = {
                    projMatrix.m0, projMatrix.m1, projMatrix.m2, projMatrix.m3,
                    projMatrix.m4, projMatrix.m5, projMatrix.m6, projMatrix.m7,
                    projMatrix.m8, projMatrix.m9, projMatrix.m10, projMatrix.m11,
                    projMatrix.m12, projMatrix.m13, projMatrix.m14, projMatrix.m15
                };

                // Extract the object's local Transform into a 4x4 matrix
                auto position = transform->GetGlobalPosition();
                auto global_scale = transform->GetGlobalScale();
                float translation[3] = { position.x, position.y, 0.0f };
                float rotation[3] = { 0.0f, 0.0f, transform->GetGlobalRotation() };
                float scale[3] = { global_scale.x, global_scale.y, 1.0f };
                
                float objMatrix[16];
                ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, objMatrix);

                // Draw and interact with the Gizmo
                // We use WORLD mode for 2D because local rotation can skew 2D scaling weirdly
                ImGuizmo::Manipulate(view, proj, currentGizmoOperation, ImGuizmo::WORLD, objMatrix);

                // Undo / Redo gizmo tracking
                static bool wasUsingGizmo = false;
                static Transform2dState initialTransformState;

                if (ImGuizmo::IsUsing()) {
                    if (!wasUsingGizmo) {
                        // The user JUST clicked on the Gizmo! Save the current LOCAL state.
                        initialTransformState = { transform->position, transform->rotation, transform->scale };
                        wasUsingGizmo = true;
                    }

                    // Apply the continuous movement to the transform
                    ImGuizmo::DecomposeMatrixToComponents(objMatrix, translation, rotation, scale);
                    
                    // Let the Transform component calculate the proper local math!
                    transform->SetGlobalPosition({ translation[0], translation[1] });
                    transform->SetGlobalRotation(rotation[2]);
                    transform->SetGlobalScale({ scale[0], scale[1] });

                } else {
                    if (wasUsingGizmo) {
                        // The user JUST released the mouse click! The drag is over.
                        // We record this movement as a single undoable action using LOCAL coordinates.
                        Transform2dState currentState = { transform->position, transform->rotation, transform->scale };

                        // We check if it actually moved to avoid empty commands
                        if (initialTransformState.position.x != currentState.position.x ||
                            initialTransformState.position.y != currentState.position.y ||
                            initialTransformState.rotation != currentState.rotation ||
                            initialTransformState.scale.x != currentState.scale.x ||
                            initialTransformState.scale.y != currentState.scale.y)
                        {
                            CommandHistory::AddCommand(std::make_unique<Transform2dCommand>(selectedObject, initialTransformState, currentState));
                        }

                        wasUsingGizmo = false;
                    }
                }
            }
        }

        static bool isPainting = false;
        static std::vector<int> initialLayerData;
        static TileMap* activePaintMap = nullptr;
        static int activePaintLayer = 0;

        // Rectangle Tool State
        static int rectStartX = 0, rectStartY = 0;
        static int rectCurrentX = 0, rectCurrentY = 0;

        static bool isCapturingStamp = false;
        static std::vector<int> stampBuffer;
        static int stampWidth = 0, stampHeight = 0;

        // We only allow ImGuizmo to block scene interactions if it is currently visible
        bool isGizmoBlocking = showGizmo && ImGuizmo::IsOver();

        // Mouse picking logic
        if (!isGizmoBlocking && isImageHovered) {
            // Calculate World Position of the mouse
            ImVec2 mousePosAbsolute = ImGui::GetMousePos();
            Vector2 mousePosRel = {
                mousePosAbsolute.x - imagePosAbsolute.x,
                mousePosAbsolute.y - imagePosAbsolute.y
            };
            Vector2 renderTexturePos = {
                (mousePosRel.x / drawSize.x) * texWidth,
                (mousePosRel.y / drawSize.y) * texHeight
            };
            Vector2 worldPos = GetScreenToWorld2D(renderTexturePos, camera.GetCamera());

            bool handledAsPaint = false;

            // Paint mode logic
            if (selectedObject) {
                if (auto tileMap = selectedObject->GetComponent<TileMap>()) {
                    auto transform = selectedObject->GetComponent<Transform2d>();
                    if (transform) {
                        handledAsPaint = true; // Block standard object picking

                        // Define what we are going to paint based on the active tool
                        int paintID = (currentTileTool == TileTool::Eraser) ? -1 : selectedTileID;

                        // Calculate grid coordinates upfront
                        auto position = transform->GetGlobalPosition();
                        float localX = worldPos.x - position.x;
                        float localY = worldPos.y - position.y;
                        float scaledWidth = tileMap->tileSize.x * transform->scale.x;
                        float scaledHeight = tileMap->tileSize.y * transform->scale.y;

                        int gridX = -1;
                        int gridY = -1;

                        if (localX >= 0 && localY >= 0) {
                            gridX = (int)(localX / scaledWidth);
                            gridY = (int)(localY / scaledHeight);
                        }

                        // ----------------------------------------------------
                        // RIGHT-CLICK: GLOBAL STAMP CAPTURE
                        // ----------------------------------------------------
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                            if (gridX >= 0 && gridY >= 0) {
                                isCapturingStamp = true;
                                rectStartX = gridX;
                                rectStartY = gridY;
                                rectCurrentX = gridX;
                                rectCurrentY = gridY;
                            }
                        }

                        if (isCapturingStamp && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                            rectCurrentX = std::max(0, gridX);
                            rectCurrentY = std::max(0, gridY);
                        }

                        if (isCapturingStamp && ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
                            isCapturingStamp = false;
                            
                            int minX = std::max(0, std::min(rectStartX, rectCurrentX));
                            int maxX = std::min(tileMap->gridWidth - 1, std::max(rectStartX, rectCurrentX));
                            int minY = std::max(0, std::min(rectStartY, rectCurrentY));
                            int maxY = std::min(tileMap->gridHeight - 1, std::max(rectStartY, rectCurrentY));

                            stampWidth = (maxX - minX) + 1;
                            stampHeight = (maxY - minY) + 1;
                            
                            if (stampWidth > 0 && stampHeight > 0) {
                                stampBuffer.resize(stampWidth * stampHeight);
                                for (int y = 0; y < stampHeight; ++y) {
                                    for (int x = 0; x < stampWidth; ++x) {
                                        stampBuffer[y * stampWidth + x] = tileMap->GetTile(selectedLayer, minX + x, minY + y);
                                    }
                                }
                                // Auto-switch to Stamp tool upon successful capture
                                currentTileTool = TileTool::Stamp;
                            }
                        }

                        // ----------------------------------------------------
                        // LEFT-CLICK: PAINTING ACTIONS
                        // ----------------------------------------------------
                        if (!isCapturingStamp && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                            isPainting = true;
                            activePaintMap = tileMap;
                            activePaintLayer = selectedLayer;

                            if (currentTileTool == TileTool::Eyedropper) {
                                if (gridX >= 0 && gridY >= 0) {
                                    int pickedID = tileMap->GetTile(activePaintLayer, gridX, gridY);
                                    selectedTileID = pickedID; 
                                    currentTileTool = (selectedTileID == -1) ? TileTool::Eraser : TileTool::Brush;
                                }
                                isPainting = false;
                                activePaintMap = nullptr;
                            } 
                            else if (currentTileTool == TileTool::Rectangle) {
                                initialLayerData = tileMap->GetLayerData(activePaintLayer);
                                rectStartX = gridX;
                                rectStartY = gridY;
                                rectCurrentX = gridX;
                                rectCurrentY = gridY;
                            }
                            else if (currentTileTool == TileTool::Stamp) {
                                initialLayerData = tileMap->GetLayerData(activePaintLayer);
                            }
                            else {
                                initialLayerData = tileMap->GetLayerData(activePaintLayer);

                                if (currentTileTool == TileTool::Bucket) {
                                    if (gridX >= 0 && gridY >= 0) {
                                        tileMap->FloodFill(activePaintLayer, gridX, gridY, paintID);
                                    }
                                    isPainting = false;
                                    std::vector<int> currentData = activePaintMap->GetLayerData(activePaintLayer);
                                    if (initialLayerData != currentData) {
                                        CommandHistory::AddCommand(std::make_unique<TileMapPaintCommand>(activePaintMap, activePaintLayer, initialLayerData, currentData));
                                    }
                                    activePaintMap = nullptr;
                                }
                            }
                        }
                        
                        // Continuous drag logic
                        if (isPainting && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                            if (currentTileTool == TileTool::Rectangle) {
                                rectCurrentX = gridX;
                                rectCurrentY = gridY;
                            }
                            else if (currentTileTool == TileTool::Stamp) {
                                if (!stampBuffer.empty() && gridX >= 0 && gridY >= 0) {
                                    for (int y = 0; y < stampHeight; ++y) {
                                        for (int x = 0; x < stampWidth; ++x) {
                                            int targetX = gridX + x;
                                            int targetY = gridY + y;
                                            if (targetX < tileMap->gridWidth && targetY < tileMap->gridHeight) {
                                                tileMap->SetTile(activePaintLayer, targetX, targetY, stampBuffer[y * stampWidth + x]);
                                            }
                                        }
                                    }
                                }
                            }
                            else if (currentTileTool != TileTool::Bucket && currentTileTool != TileTool::Eyedropper) {
                                if (gridX >= 0 && gridY >= 0) {
                                    tileMap->SetTile(activePaintLayer, gridX, gridY, paintID);
                                }
                            }
                        }

                        // ----------------------------------------------------
                        // TOOL VISUAL PREVIEWS
                        // ----------------------------------------------------
                        auto DrawWorldRect = [&](int x1, int y1, int x2, int y2, ImU32 fill, ImU32 outline) {
                            Vector2 tlWorld = { position.x + x1 * scaledWidth, position.y + y1 * scaledHeight };
                            Vector2 brWorld = { position.x + (x2 + 1) * scaledWidth, position.y + (y2 + 1) * scaledHeight };
                            Vector2 tlTex = GetWorldToScreen2D(tlWorld, camera.GetCamera());
                            Vector2 brTex = GetWorldToScreen2D(brWorld, camera.GetCamera());
                            ImVec2 p_min = ImVec2(imagePosAbsolute.x + (tlTex.x / texWidth) * drawSize.x, imagePosAbsolute.y + (tlTex.y / texHeight) * drawSize.y);
                            ImVec2 p_max = ImVec2(imagePosAbsolute.x + (brTex.x / texWidth) * drawSize.x, imagePosAbsolute.y + (brTex.y / texHeight) * drawSize.y);
                            ImGui::GetWindowDrawList()->AddRectFilled(p_min, p_max, fill); 
                            ImGui::GetWindowDrawList()->AddRect(p_min, p_max, outline, 0.0f, 0, 2.0f);
                        };

                        // Stamp Capture Preview (Blue)
                        if (isCapturingStamp && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                            int minX = std::max(0, std::min(rectStartX, rectCurrentX));
                            int maxX = std::min(tileMap->gridWidth - 1, std::max(rectStartX, rectCurrentX));
                            int minY = std::max(0, std::min(rectStartY, rectCurrentY));
                            int maxY = std::min(tileMap->gridHeight - 1, std::max(rectStartY, rectCurrentY));
                            DrawWorldRect(minX, minY, maxX, maxY, IM_COL32(50, 150, 255, 80), IM_COL32(50, 150, 255, 255));
                        }
                        // Stamp Paste Preview (Yellow)
                        else if (!isCapturingStamp && !isPainting && currentTileTool == TileTool::Stamp && !stampBuffer.empty() && gridX >= 0 && gridY >= 0) {
                            DrawWorldRect(gridX, gridY, gridX + stampWidth - 1, gridY + stampHeight - 1, IM_COL32(230, 200, 50, 80), IM_COL32(230, 200, 50, 255));
                        }
                        // Rectangle Shape Preview (Orange)
                        else if (isPainting && currentTileTool == TileTool::Rectangle && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                            int minX = std::max(0, std::min(rectStartX, rectCurrentX));
                            int maxX = std::min(tileMap->gridWidth - 1, std::max(rectStartX, rectCurrentX));
                            int minY = std::max(0, std::min(rectStartY, rectCurrentY));
                            int maxY = std::min(tileMap->gridHeight - 1, std::max(rectStartY, rectCurrentY));
                            DrawWorldRect(minX, minY, maxX, maxY, IM_COL32(230, 115, 25, 80), IM_COL32(230, 115, 25, 255));
                        }
                    }
                }
            }

            // Object picking logic
            // Only trigger if we are NOT painting a TileMap, and we just clicked once
            if (!handledAsPaint && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                GameObject* pickedObject = activeScene.PickObject(worldPos);
                selectedObject = pickedObject;
            }
        }

        // We check this globally so that if the user drags their mouse outside the Scene Window
        // and releases the click, we still capture the command!
        if (isPainting && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            isPainting = false;
            
            if (activePaintMap) {
                // Apply Rectangle Tool fill
                if (currentTileTool == TileTool::Rectangle) {
                    int minX = std::max(0, std::min(rectStartX, rectCurrentX));
                    int maxX = std::min(activePaintMap->gridWidth - 1, std::max(rectStartX, rectCurrentX));
                    int minY = std::max(0, std::min(rectStartY, rectCurrentY));
                    int maxY = std::min(activePaintMap->gridHeight - 1, std::max(rectStartY, rectCurrentY));

                    int paintID = selectedTileID; // Works as box-eraser if -1

                    for (int y = minY; y <= maxY; ++y) {
                        for (int x = minX; x <= maxX; ++x) {
                            activePaintMap->SetTile(activePaintLayer, x, y, paintID);
                        }
                    }
                }

                std::vector<int> currentData = activePaintMap->GetLayerData(activePaintLayer);
                
                // Only add a command if the user actually modified at least one tile
                if (initialLayerData != currentData) {
                    CommandHistory::AddCommand(std::make_unique<TileMapPaintCommand>(activePaintMap, activePaintLayer, initialLayerData, currentData));
                }
            }
            activePaintMap = nullptr;
        }
    }
    
    ImGui::End();
    ImGui::PopStyleVar();
}
