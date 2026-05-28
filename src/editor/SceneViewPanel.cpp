#include "SceneViewPanel.hpp"
#include <rlImGui.h>
#include "ImGuizmo.h"
#include <raymath.h>
#include "extras/IconsFontAwesome6.h"
#include "commands/CommandHistory.hpp"
#include "commands/Transform2dCommand.hpp"
#include "commands/ModifyComponentCommand.hpp"
#include "graphics/ShapeRenderer.hpp"
#include <graphics/TileMap.hpp>
#include "commands/TileMapPaintCommand.hpp"
#include "physics/Transform3d.hpp"
#include "TilePalettePanel.hpp" 

void SceneViewPanel::Draw(RenderTexture2D& sceneTexture, EditorCamera& camera, Scene& activeScene, GameObject*& selectedObject, int selectedTileID, int selectedLayer, TileTool currentTileTool, EditorViewMode& currentViewMode) {    
    // Remove inner margins (padding) so the render texture touches the window borders
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Scene View");

    // Track active window focus to switch context modes seamlessly
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

        // ----------------------------------------------------
        // OUTLINE SELECTION 2D (Only for 2D objects)
        // ----------------------------------------------------
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

        // ----------------------------------------------------
        // GIZMO TOOLBAR
        // ----------------------------------------------------
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

        // ----------------------------------------------------
        // GIZMO RENDERING (Hybrid 2D / 3D Implementation)
        // ----------------------------------------------------
        if (selectedObject && showGizmo) {
            auto transform3d = selectedObject->GetComponent<Transform3d>();
            auto transform2d = selectedObject->GetComponent<Transform2d>();

            if (transform3d) {
                // ==========================================
                // 3D GIZMO
                // ==========================================
                ImGuizmo::SetOrthographic(false); // Enable Perspective projection for 3D
                ImGuizmo::SetDrawlist();
                ImGuizmo::SetRect(imagePosAbsolute.x, imagePosAbsolute.y, drawSize.x, drawSize.y);

                Camera3D cam3d = camera.GetCamera3D();
                Matrix viewMatrix = GetCameraMatrix(cam3d);
                
                // Calculate real aspect ratio for the 3D projection
                float aspect = drawSize.x / drawSize.y;
                Matrix projMatrix = MatrixPerspective(cam3d.fovy * DEG2RAD, aspect, 0.01f, 1000.0f);

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

                // --- EULER ANGLE FIX ---
                // We use ImGuizmo to generate the local matrix to ensure 
                // that Euler angles are calculated using ITS mathematical convention.
                float translation[3] = { transform3d->position.x, transform3d->position.y, transform3d->position.z };
                float rotation[3]    = { transform3d->rotation.x, transform3d->rotation.y, transform3d->rotation.z };
                float scale[3]       = { transform3d->scale.x, transform3d->scale.y, transform3d->scale.z };
                
                float localMatrixFloat[16];
                ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, localMatrixFloat);

                // Safe reconversion to Raylib's Matrix structure
                Matrix localMat;
                localMat.m0 = localMatrixFloat[0]; localMat.m1 = localMatrixFloat[1]; localMat.m2 = localMatrixFloat[2]; localMat.m3 = localMatrixFloat[3];
                localMat.m4 = localMatrixFloat[4]; localMat.m5 = localMatrixFloat[5]; localMat.m6 = localMatrixFloat[6]; localMat.m7 = localMatrixFloat[7];
                localMat.m8 = localMatrixFloat[8]; localMat.m9 = localMatrixFloat[9]; localMat.m10= localMatrixFloat[10]; localMat.m11= localMatrixFloat[11];
                localMat.m12= localMatrixFloat[12]; localMat.m13= localMatrixFloat[13]; localMat.m14= localMatrixFloat[14]; localMat.m15= localMatrixFloat[15];

                // Apply parent hierarchy if it exists
                Matrix globalMat = localMat;
                if (selectedObject->GetParent() && selectedObject->GetParent()->GetComponent<Transform3d>()) {
                    Matrix parentGlobal = selectedObject->GetParent()->GetComponent<Transform3d>()->GetGlobalMatrix();
                    globalMat = MatrixMultiply(localMat, parentGlobal);
                }

                // Extract the Global matrix for the Gizmo
                float objMatrix[16];
                objMatrix[0] = globalMat.m0; objMatrix[1] = globalMat.m1; objMatrix[2] = globalMat.m2; objMatrix[3] = globalMat.m3;
                objMatrix[4] = globalMat.m4; objMatrix[5] = globalMat.m5; objMatrix[6] = globalMat.m6; objMatrix[7] = globalMat.m7;
                objMatrix[8] = globalMat.m8; objMatrix[9] = globalMat.m9; objMatrix[10]= globalMat.m10; objMatrix[11]= globalMat.m11;
                objMatrix[12]= globalMat.m12; objMatrix[13]= globalMat.m13; objMatrix[14]= globalMat.m14; objMatrix[15]= globalMat.m15;

                // In 3D, scaling should often be done in LOCAL mode to avoid strange deformations (shearing)
                ImGuizmo::MODE gizmoMode = (currentGizmoOperation == ImGuizmo::SCALE) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
                ImGuizmo::Manipulate(view, proj, currentGizmoOperation, gizmoMode, objMatrix);

                // Undo / Redo tracking for 3D
                static bool wasUsingGizmo3D = false;
                static nlohmann::json initialTransform3dState;

                if (ImGuizmo::IsUsing()) {
                    if (!wasUsingGizmo3D) {
                        initialTransform3dState = transform3d->Serialize();
                        wasUsingGizmo3D = true;
                    }

                    // Retrieve the global result from ImGuizmo
                    Matrix newGlobal;
                    newGlobal.m0 = objMatrix[0]; newGlobal.m1 = objMatrix[1]; newGlobal.m2 = objMatrix[2]; newGlobal.m3 = objMatrix[3];
                    newGlobal.m4 = objMatrix[4]; newGlobal.m5 = objMatrix[5]; newGlobal.m6 = objMatrix[6]; newGlobal.m7 = objMatrix[7];
                    newGlobal.m8 = objMatrix[8]; newGlobal.m9 = objMatrix[9]; newGlobal.m10= objMatrix[10]; newGlobal.m11= objMatrix[11];
                    newGlobal.m12= objMatrix[12]; newGlobal.m13= objMatrix[13]; newGlobal.m14= objMatrix[14]; newGlobal.m15= objMatrix[15];

                    Matrix newLocal = newGlobal;
                    
                    // If the object has a parent, we subtract its influence to get the local matrix back
                    if (selectedObject->GetParent() && selectedObject->GetParent()->GetComponent<Transform3d>()) {
                        Matrix parentGlobalInv = MatrixInvert(selectedObject->GetParent()->GetComponent<Transform3d>()->GetGlobalMatrix());
                        newLocal = MatrixMultiply(newGlobal, parentGlobalInv);
                    }

                    float localFloat[16];
                    localFloat[0] = newLocal.m0; localFloat[1] = newLocal.m1; localFloat[2] = newLocal.m2; localFloat[3] = newLocal.m3;
                    localFloat[4] = newLocal.m4; localFloat[5] = newLocal.m5; localFloat[6] = newLocal.m6; localFloat[7] = newLocal.m7;
                    localFloat[8] = newLocal.m8; localFloat[9] = newLocal.m9; localFloat[10]= newLocal.m10; localFloat[11]= newLocal.m11;
                    localFloat[12]= newLocal.m12; localFloat[13]= newLocal.m13; localFloat[14]= newLocal.m14; localFloat[15]= newLocal.m15;

                    // Final decomposition into the component's values
                    ImGuizmo::DecomposeMatrixToComponents(localFloat, translation, rotation, scale);

                    transform3d->position = { translation[0], translation[1], translation[2] };
                    transform3d->rotation = { rotation[0], rotation[1], rotation[2] };
                    transform3d->scale = { scale[0], scale[1], scale[2] };

                } else {
                    if (wasUsingGizmo3D) {
                        nlohmann::json currentState = transform3d->Serialize();
                        if (initialTransform3dState != currentState) {
                            CommandHistory::AddCommand(std::make_unique<ModifyComponentCommand>(transform3d, initialTransform3dState, currentState));
                        }
                        wasUsingGizmo3D = false;
                    }
                }
            } 
            else if (transform2d) {
                // ==========================================
                // 2D GIZMO
                // ==========================================
                ImGuizmo::SetOrthographic(true); // We are in 2D
                ImGuizmo::SetDrawlist();
                ImGuizmo::SetRect(imagePosAbsolute.x, imagePosAbsolute.y, drawSize.x, drawSize.y);

                // Extract Raylib camera matrices
                Camera2D cam2d = camera.GetCamera2D();
                Matrix viewMatrix = GetCameraMatrix2D(cam2d);
                Matrix projMatrix = MatrixOrtho(0.0f, texWidth, texHeight, 0.0f, -1.0f, 1.0f);

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
                auto position = transform2d->GetGlobalPosition();
                auto global_scale = transform2d->GetGlobalScale();
                float translation[3] = { position.x, position.y, 0.0f };
                float rotation[3] = { 0.0f, 0.0f, transform2d->GetGlobalRotation() };
                float scale[3] = { global_scale.x, global_scale.y, 1.0f };
                
                float objMatrix[16];
                ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, objMatrix);

                // Draw and interact with the Gizmo
                // We use WORLD mode for 2D because local rotation can skew 2D scaling weirdly
                ImGuizmo::Manipulate(view, proj, currentGizmoOperation, ImGuizmo::WORLD, objMatrix);

                // Undo / Redo gizmo tracking
                static bool wasUsingGizmo2D = false;
                static Transform2dState initialTransformState;

                if (ImGuizmo::IsUsing()) {
                    if (!wasUsingGizmo2D) {
                        // The user JUST clicked on the Gizmo! Save the current LOCAL state.
                        initialTransformState = { transform2d->position, transform2d->rotation, transform2d->scale };
                        wasUsingGizmo2D = true;
                    }

                    // Apply the continuous movement to the transform
                    ImGuizmo::DecomposeMatrixToComponents(objMatrix, translation, rotation, scale);
                    
                    // Let the Transform component calculate the proper local math!
                    transform2d->SetGlobalPosition({ translation[0], translation[1] });
                    transform2d->SetGlobalRotation(rotation[2]);
                    transform2d->SetGlobalScale({ scale[0], scale[1] });

                } else {
                    if (wasUsingGizmo2D) {
                        // The user JUST released the mouse click! The drag is over.
                        // We record this movement as a single undoable action using LOCAL coordinates.
                        Transform2dState currentState = { transform2d->position, transform2d->rotation, transform2d->scale };

                        // We check if it actually moved to avoid empty commands
                        if (initialTransformState.position.x != currentState.position.x ||
                            initialTransformState.position.y != currentState.position.y ||
                            initialTransformState.rotation != currentState.rotation ||
                            initialTransformState.scale.x != currentState.scale.x ||
                            initialTransformState.scale.y != currentState.scale.y)
                        {
                            CommandHistory::AddCommand(std::make_unique<Transform2dCommand>(selectedObject, initialTransformState, currentState));
                        }
                        wasUsingGizmo2D = false;
                    }
                }
            }
        }

        // ----------------------------------------------------
        // TILEMAP PAINTING & OBJECT PICKING (2D Only for now)
        // ----------------------------------------------------
        static bool isPainting = false;
        static std::vector<int> initialLayerData;
        static TileMap* activePaintMap = nullptr;
        static int activePaintLayer = 0;

        // We only allow ImGuizmo to block scene interactions if it is currently visible
        bool isGizmoBlocking = showGizmo && ImGuizmo::IsOver();

        // Mouse picking logic
        if (!isGizmoBlocking && isImageHovered) {
            ImVec2 mousePosAbsolute = ImGui::GetMousePos();
            Vector2 mousePosRel = {
                mousePosAbsolute.x - imagePosAbsolute.x,
                mousePosAbsolute.y - imagePosAbsolute.y
            };
            Vector2 renderTexturePos = {
                (mousePosRel.x / drawSize.x) * texWidth,
                (mousePosRel.y / drawSize.y) * texHeight
            };
            
            // Only perform 2D mouse picking for now
            Vector2 worldPos = GetScreenToWorld2D(renderTexturePos, camera.GetCamera2D());

            bool handledAsPaint = false;

            // Paint mode logic
            if (selectedObject) {
                if (auto tileMap = selectedObject->GetComponent<TileMap>()) {
                    auto transform = selectedObject->GetComponent<Transform2d>();
                    if (transform) {
                        handledAsPaint = true; // Block standard object picking

                        // Define what we are going to paint based on the active tool
                        int paintID = (currentTileTool == TileTool::Eraser) ? -1 : selectedTileID;

                        // The stroke starts, save the initial state
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                            isPainting = true;
                            activePaintMap = tileMap;
                            activePaintLayer = selectedLayer;
                            // Make a copy of the active layer for Undo/Redo (Fix: Use activePaintLayer, not 0)
                            initialLayerData = tileMap->GetLayerData(activePaintLayer);

                            // Handle Bucket Tool execution immediately on click
                            if (currentTileTool == TileTool::Bucket) {
                                auto position = transform->GetGlobalPosition();
                                float localX = worldPos.x - position.x;
                                float localY = worldPos.y - position.y;
                                float scaledWidth = tileMap->tileSize.x * transform->scale.x;
                                float scaledHeight = tileMap->tileSize.y * transform->scale.y;

                                if (localX >= 0 && localY >= 0) {
                                    int gridX = (int)(localX / scaledWidth);
                                    int gridY = (int)(localY / scaledHeight);
                                    
                                    // Execute the flood fill on the active layer
                                    tileMap->FloodFill(activePaintLayer, gridX, gridY, paintID);
                                }

                                // The bucket is an instant action, so we finalize the stroke immediately
                                isPainting = false;
                                std::vector<int> currentData = activePaintMap->GetLayerData(activePaintLayer);
                                
                                if (initialLayerData != currentData) {
                                    CommandHistory::AddCommand(std::make_unique<TileMapPaintCommand>(activePaintMap, activePaintLayer, initialLayerData, currentData));
                                }
                                activePaintMap = nullptr;
                            }
                        }
                        
                        // Handle Brush and Eraser (Continuous drag)
                        if (isPainting && currentTileTool != TileTool::Bucket && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                            
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
                                
                                // Set the tile on the active layer
                                tileMap->SetTile(activePaintLayer, gridX, gridY, paintID);
                            }
                        }
                    }
                }
            }

            // Object picking logic
            // Only trigger if we are NOT painting a TileMap, and we just clicked once
            if (!handledAsPaint && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                
                // 1. First, try to pick a 2D object
                GameObject* pickedObject = activeScene.PickObject(worldPos);
                
                // 2. If nothing was picked in 2D, fallback to 3D Raycasting
                if (!pickedObject && activeScene.Has3DCamera()) {
                    Camera3D cam3d = camera.GetCamera3D();
                    
                    // Convert mouse coordinates on the render texture to Normalized Device Coordinates (NDC)
                    // Range: [-1, 1] for both X and Y
                    float nx = (2.0f * renderTexturePos.x) / texWidth - 1.0f;
                    float ny = 1.0f - (2.0f * renderTexturePos.y) / texHeight; 
                    
                    // Extract Inverse View and Inverse Projection matrices
                    Matrix invView = MatrixInvert(GetCameraMatrix(cam3d));
                    Matrix invProj = MatrixInvert(MatrixPerspective(cam3d.fovy * DEG2RAD, texWidth / texHeight, 0.01f, 1000.0f));
                    
                    // Bulletproof manual Unproject function (Handles Perspective 'W' Divide properly)
                    auto Unproject = [&](Vector3 ndc) -> Vector3 {
                        // Step 1: NDC to Camera Space (Inverse Projection)
                        float cx = invProj.m0*ndc.x + invProj.m4*ndc.y + invProj.m8*ndc.z + invProj.m12;
                        float cy = invProj.m1*ndc.x + invProj.m5*ndc.y + invProj.m9*ndc.z + invProj.m13;
                        float cz = invProj.m2*ndc.x + invProj.m6*ndc.y + invProj.m10*ndc.z + invProj.m14;
                        float cw = invProj.m3*ndc.x + invProj.m7*ndc.y + invProj.m11*ndc.z + invProj.m15;
                        
                        // Crucial Perspective Divide
                        if (cw != 0.0f) {
                            cx /= cw;
                            cy /= cw;
                            cz /= cw;
                        }
                        
                        // Step 2: Camera Space to World Space (Inverse View)
                        float wx = invView.m0*cx + invView.m4*cy + invView.m8*cz + invView.m12;
                        float wy = invView.m1*cx + invView.m5*cy + invView.m9*cz + invView.m13;
                        float wz = invView.m2*cx + invView.m6*cy + invView.m10*cz + invView.m14;
                        
                        return { wx, wy, wz };
                    };
                    
                    // Unproject near and far clipping planes
                    Vector3 nearPoint = Unproject({ nx, ny, -1.0f });
                    Vector3 farPoint  = Unproject({ nx, ny,  1.0f });
                    
                    // Construct the Ray
                    Ray ray;
                    ray.position = cam3d.position; // Ray starts at the camera lens
                    ray.direction = Vector3Normalize(Vector3Subtract(farPoint, nearPoint)); // Points deep into the scene
                    
                    // Perform the intersection test
                    pickedObject = activeScene.PickObject3D(ray);
                }

                // Apply the selection (will be nullptr if user clicked the empty void)
                selectedObject = pickedObject;
            }
        }

        // We check this globally so that if the user drags their mouse outside the Scene Window
        // and releases the click, we still capture the command!
        if (isPainting && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            isPainting = false;
            
            if (activePaintMap) {
                // Fix: Fetch current data using activePaintLayer, not 0
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
