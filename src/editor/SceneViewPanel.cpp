#include "SceneViewPanel.hpp"
#include <rlImGui.h>
#include "ImGuizmo.h"
#include <raymath.h>
#include "extras/IconsFontAwesome6.h"
#include "commands/CommandHistory.hpp"
#include "commands/Transform2dCommand.hpp"
#include "graphics/ShapeRenderer.hpp"
#include <graphics/TileMap.hpp>
#include "commands/TileMapPaintCommand.hpp"

void SceneViewPanel::Draw(RenderTexture2D& sceneTexture, EditorCamera& camera, Scene& activeScene, GameObject*& selectedObject, int selectedTileID, int selectedLayer, EditorViewMode& currentViewMode) {    
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
                Camera2D cam2d = camera.GetCamera2D();
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

        // Gizmos
        if (selectedObject) {
            auto transform = selectedObject->GetComponent<Transform2d>();
            if (transform) {
                // Setup ImGuizmo environment
                ImGuizmo::SetOrthographic(true); // We are in 2D
                ImGuizmo::SetDrawlist();
                ImGuizmo::SetRect(imagePosAbsolute.x, imagePosAbsolute.y, drawSize.x, drawSize.y);

                // Extract Raylib camera matrices
                Camera2D cam2d = camera.GetCamera2D();
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

        // Mouse picking logic
        if (!ImGuizmo::IsOver() && isImageHovered) {
            // Calculate World Position of the mouse (same as before)
            ImVec2 mousePosAbsolute = ImGui::GetMousePos();
            Vector2 mousePosRel = {
                mousePosAbsolute.x - imagePosAbsolute.x,
                mousePosAbsolute.y - imagePosAbsolute.y
            };
            Vector2 renderTexturePos = {
                (mousePosRel.x / drawSize.x) * texWidth,
                (mousePosRel.y / drawSize.y) * texHeight
            };
            Vector2 worldPos = GetScreenToWorld2D(renderTexturePos, camera.GetCamera2D());

            bool handledAsPaint = false;

            // Paint mode logic
            if (selectedObject) {
                if (auto tileMap = selectedObject->GetComponent<TileMap>()) {
                    auto transform = selectedObject->GetComponent<Transform2d>();
                    if (transform) {
                        handledAsPaint = true; // Block standard object picking

                        // The stroke starts, save the initial state
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                            isPainting = true;
                            activePaintMap = tileMap;
                            activePaintLayer = selectedLayer;
                            initialLayerData = tileMap->GetLayerData(0);
                        }
                        
                        // Use IsMouseDown (not IsMouseClicked) to allow click-and-drag painting!
                        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                            
                            // Calculate local position relative to the TileMap's origin
                            auto position = transform->GetGlobalPosition();
                            float localX = worldPos.x - position.x;
                            float localY = worldPos.y - position.y;
                            
                            // Account for the object's scale
                            float scaledTileWidth = tileMap->tileSize.x * transform->scale.x;
                            float scaledTileHeight = tileMap->tileSize.y * transform->scale.y;
                            
                            // Prevent negative index wrapping if mouse is above/left of the map
                            if (localX >= 0 && localY >= 0) {
                                int gridX = (int)(localX / scaledTileWidth);
                                int gridY = (int)(localY / scaledTileHeight);
                                
                                // Set the tile on Layer 0 (Base Layer). 
                                // The -1 fallback handles the "eraser" if you clicked outside the palette.
                                tileMap->SetTile(activePaintLayer, gridX, gridY, selectedTileID);
                            }
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
                std::vector<int> currentData = activePaintMap->GetLayerData(0);
                
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
