# This file is strictly for Visual Studio Code (or other IDEs) to provide type hinting 
# and autocompletion. The "..." (Ellipsis) indicate to Python that the actual 
# implementation is handled natively in C++.

from enum import Enum
from typing import TypeVar, Type, Optional, List, overload, Tuple, Dict, Callable, Any

# Type variable for smart autocompletion in the get_component method.
# It ensures that getting a Transform2d actually returns a Transform2d type in the IDE.
T = TypeVar('T', bound='Component')


class Debug:
    @staticmethod
    def log(msg: str) -> None:
        """
        Prints a message directly to the C++ engine console.
        Bypasses Python's standard stdout to prevent crashes with graphics libraries.
        """
        ...

    @staticmethod
    def error(msg: str) -> None:
        """
        Prints an error directly to the C++ engine console.
        Bypasses Python's standard stdout to prevent crashes with graphics libraries.
        """
        ...


def set_pixel_art_mode(enable: bool) -> None:
    """
    Globally configures texture filtering. 
    Call this BEFORE instantiating GameObjects with sprites.
    - True: Nearest-neighbor filtering (Sharp, blocky, perfect for pixel art).
    - False: Bilinear filtering (Smooth, interpolated edges).
    """
    ...


def is_pixel_art_mode() -> bool:
    """Returns whether the engine is currently in pixel art mode."""
    ...


class GameObject:
    name: str

    # Tag used to group and identify objects (e.g., "Enemy", "Player", "Projectile")
    # Defaults to "Untagged"
    tag: str

    # Allow enabling or disabling the GameObject and its components
    is_active: bool
    
    # Read-only unique identifier
    @property
    def id(self) -> int: ...

    def is_active_in_hierarchy(self) -> bool:
        """
        Checks if this object is currently active in the scene.
        Returns False if this object OR any of its parents are inactive.
        """
        ...

    def set_parent(self, new_parent: Optional['GameObject']) -> None:
        """Attaches this object to a new parent. Pass None to unparent (move to root)."""
        ...
        
    def get_parent(self) -> Optional['GameObject']:
        """Returns the current parent of this object, or None if it's at the root."""
        ...
        
    def get_children(self) -> List['GameObject']:
        """Returns a list of all direct children attached to this object."""
        ...

    """Represents an entity within the engine's scene."""
    @staticmethod
    def spawn(name: str) -> GameObject:
        """
        Creates a new empty GameObject in the active scene.
        
        Args:
            name: The name of the new entity.
            
        Returns:
            The newly created GameObject instance.
        """
        ...

    @staticmethod
    def instantiate(prefab_path: str) -> Optional[GameObject]:
        """
        Instantiates a new GameObject from a saved JSON prefab file.
        
        Args:
            prefab_path: The relative path to the .json prefab file (e.g., 'assets/prefabs/Bullet.json').
            
        Returns:
            The newly instantiated GameObject instance, or None if the file could not be loaded.
        """
        ...

    def destroy(self) -> None:
        """
        Marks this GameObject for destruction. 
        It will be safely removed from memory at the end of the current C++ frame.
        """
        ...
    
    def get_component(self, type_obj: Type[T]) -> Optional[T]:
        """
        Retrieves a component attached to this GameObject.
        
        Args:
            type_obj: The class type of the component to retrieve (e.g., Transform2d).
            
        Returns:
            The component instance if found, otherwise None.
        """
        ...

    def add_component(self, type_obj: Type[T], *args) -> T:
        """
        Dynamically instantiates and attaches a new component to this GameObject.
        
        Args:
            type_obj: The class type of the component to create.
            *args: Variable arguments passed directly to the C++ constructor of the component.
            
        Returns:
            The newly created and attached component instance.
        """
        ...


class Vector2:
    """A 2D vector representing mathematical points or directions."""
    x: float
    y: float
    
    def __init__(self, x: float = 0.0, y: float = 0.0) -> None:
        """Initializes the vector with the given x and y coordinates."""
        ...

    def __add__(self, other: 'Vector2') -> 'Vector2':
        """Adds another Vector2 to this one and returns a new Vector2."""
        ...

    def __sub__(self, other: 'Vector2') -> 'Vector2':
        """Subtracts another Vector2 from this one and returns a new Vector2."""
        ...

    def __mul__(self, scalar: float) -> 'Vector2':
        """Multiplies the vector by a scalar value and returns a new Vector2."""
        ...

    def __truediv__(self, scalar: float) -> 'Vector2':
        """Divides the vector by a scalar value and returns a new Vector2. Prevents division by zero."""
        ...

    def length(self) -> float:
        """Calculates and returns the magnitude (length) of the vector."""
        ...

    def distance_to(self, other: 'Vector2') -> float:
        """Calculates the exact distance between this vector and another Vector2."""
        ...

    def normalized(self) -> 'Vector2':
        """Returns a new Vector2 with a length of exactly 1.0 (maintaining the same direction)."""
        ...

    def __repr__(self) -> str:
        """Returns a string representation of the vector for debugging (e.g., 'Vector2(1.0, 2.5)')."""
        ...


class Rectangle:
    """A standard Raylib Rectangle containing x, y, width and height"""
    x: float
    y: float
    width: float
    height: float

    def __init__(self, x: float = 0.0, y: float = 0.0, width: float = 0.0, height: float = 0.0):
        ...


class Collision2D:
    """Contains information about a physics collision event."""
    
    other: GameObject | None
    """The GameObject collided with. Will be None if the collision was with the static TileMap."""
    
    normal: Vector2
    """The direction vector of the surface hit. e.g., (0, -1) means you hit the floor."""


class Component:
    """Base class for all engine scripts and components."""
    
    __categories__: Dict[str, List[str]]
    """
    (Optional) Editor metadata to group variables in the Inspector.
    Format: {"Category Name": ["var1", "var2"]}

    Example:
        __categories__ = {
            "Movement": ["speed", "jump_force"],
            "Combat": ["health", "damage"]
        }
    """

    __tooltips__: Dict[str, str]
    """
    (Optional) Editor metadata to display help text when hovering over variables in the Inspector.
    Format: {"variable_name": "Help text"}

    Example:
        __tooltips__ = {
            "is_wanderer": "Enables or disables random wandering.",
            "speed": "Movement speed in pixels per second."
        }
    """

    __ranges__: Dict[str, Tuple[float, float]]
    """
    (Optional) Editor metadata to clamp numerical variables (int or float) in the Inspector.
    Format: {"variable_name": (min_value, max_value)}

    Example:
        __ranges__ = {
            "speed": (10.0, 500.0),
            "wander_radius": (50.0, 1000.0)
        }
    """

    __multiline__: List[str]
    """
    (Optional) Editor metadata to render string variables as a multiline text area in the Inspector.
    Format: ["var_name_1", "var_name_2"]

    Example:
        __multiline__ = ["quest_description", "npc_dialogue"]
    """

    __readonly__: List[str]
    """
    (Optional) Editor metadata to render variables as read-only (grayed out) in the Inspector.
    Useful for exposing internal state or dynamic values for debugging purposes without risking accidental edits.
    Format: ["var_name_1", "var_name_2"]

    Example:
        __readonly__ = ["current_health", "target_distance"]
    """

    __actions__: List[str]
    """
    (Optional) Editor metadata to expose Python methods as clickable buttons at the bottom of the Inspector.
    Format: ["method_name_1", "method_name_2"]

    Example:
        __actions__ = ["heal_instantly", "level_up"]
    """

    __require_components__: List[Type['Component']]
    """
    (Optional) Editor metadata to enforce Component dependencies (both Native and Python).
    If the GameObject is missing any of these components, the Inspector will display an 'Add' button.
    Format: [ComponentClass1, ComponentClass2]

    Example:
        __require_components__ = [Transform2d, RigidBody2d, PlayerController]
    """

    def __init__(self) -> None: 
        """Initializes the C++ base component memory."""
        ...
    
    @property
    def game_object(self) -> GameObject:
        """The owning GameObject of this component (Read-only)."""
        ...

    def start(self) -> None: 
        """Called once when the component is initialized."""
        ...
        
    def update(self, delta_time: float) -> None: 
        """Called every frame for logic and physics."""
        ...
        
    def on_collision(self, collision: 'Collision2D') -> None: 
        """Called when this object's collider intersects with another."""
        ...
        
    def on_animation_event(self, event_name: str) -> None:
        """
        Called automatically by an attached Animator2d when a specific frame is reached.
        Override this method in your script to handle animation events (e.g., footsteps, attacks).
        """
        ...

    def on_gui_click(self, button_name: str) -> None:
        """
        Called when a Button component on the same GameObject is clicked
        """
        ...


class Transform2d(Component):
    """Component managing spatial position, rotation, and scale."""
    
    position: Vector2 # Local position
    scale: Vector2    # Local scale
    rotation: float   # Local rotation
    z_index: int      # Local z-index order
    
    def __init__(self, x: float = 0.0, y: float = 0.0) -> None: 
        """Creates a new Transform2d component."""
        ...

    def get_global_position(self) -> Vector2:
        """Returns the true world position, taking all parent transforms into account."""
        ...
        
    def get_global_rotation(self) -> float:
        """Returns the true world rotation in degrees."""
        ...
        
    def get_global_scale(self) -> Vector2:
        """Returns the true world scale."""
        ...
        
    def get_global_z_index(self) -> int:
        """Returns the accumulated rendering order."""
        ...

    def set_global_position(self, target_global_pos: Vector2) -> None:
        """Moves the object to a world position by automatically calculating the required local position."""
        ...
        
    def set_global_rotation(self, target_global_rot: float) -> None:
        """Sets the world rotation by automatically calculating the required local rotation."""
        ...
        
    def set_global_scale(self, target_global_scale: Vector2) -> None:
        """Sets the world scale by automatically calculating the required local scale."""
        ...
    

class Keys:
    """Raylib keyboard key codes."""
    KEY_NULL: int
    KEY_APOSTROPHE: int
    KEY_COMMA: int
    KEY_MINUS: int
    KEY_PERIOD: int
    KEY_SLASH: int

    KEY_ZERO: int
    KEY_ONE: int
    KEY_TWO: int
    KEY_THREE: int
    KEY_FOUR: int
    KEY_FIVE: int
    KEY_SIX: int
    KEY_SEVEN: int
    KEY_EIGHT: int
    KEY_NINE: int

    KEY_SEMICOLON: int
    KEY_EQUAL: int

    KEY_A: int
    KEY_B: int
    KEY_C: int
    KEY_D: int
    KEY_E: int
    KEY_F: int
    KEY_G: int
    KEY_H: int
    KEY_I: int
    KEY_J: int
    KEY_K: int
    KEY_L: int
    KEY_M: int
    KEY_N: int
    KEY_O: int
    KEY_P: int
    KEY_Q: int
    KEY_R: int
    KEY_S: int
    KEY_T: int
    KEY_U: int
    KEY_V: int
    KEY_W: int
    KEY_X: int
    KEY_Y: int
    KEY_Z: int

    KEY_LEFT_BRACKET: int
    KEY_BACKSLASH: int
    KEY_RIGHT_BRACKET: int
    KEY_GRAVE: int

    KEY_SPACE: int
    KEY_ESCAPE: int
    KEY_ENTER: int
    KEY_TAB: int
    KEY_BACKSPACE: int
    KEY_INSERT: int
    KEY_DELETE: int
    RIGHT: int
    LEFT: int
    DOWN: int
    UP: int
    KEY_PAGE_UP: int
    KEY_PAGE_DOWN: int
    KEY_HOME: int
    KEY_END: int
    KEY_CAPS_LOCK: int
    KEY_SCROLL_LOCK: int
    KEY_NUM_LOCK: int
    KEY_PRINT_SCREEN: int
    KEY_PAUSE: int

    KEY_F1: int
    KEY_F2: int
    KEY_F3: int
    KEY_F4: int
    KEY_F5: int
    KEY_F6: int
    KEY_F7: int
    KEY_F8: int
    KEY_F9: int
    KEY_F10: int
    KEY_F11: int
    KEY_F12: int

    KEY_LEFT_SHIFT: int
    KEY_LEFT_CONTROL: int
    KEY_LEFT_ALT: int
    KEY_LEFT_SUPER: int
    KEY_RIGHT_SHIFT: int
    KEY_RIGHT_CONTROL: int
    KEY_RIGHT_ALT: int
    KEY_RIGHT_SUPER: int
    KEY_KB_MENU: int

    KEY_KP_0: int
    KEY_KP_1: int
    KEY_KP_2: int
    KEY_KP_3: int
    KEY_KP_4: int
    KEY_KP_5: int
    KEY_KP_6: int
    KEY_KP_7: int
    KEY_KP_8: int
    KEY_KP_9: int

    KEY_KP_DECIMAL: int
    KEY_KP_DIVIDE: int
    KEY_KP_MULTIPLY: int
    KEY_KP_SUBTRACT: int
    KEY_KP_ADD: int
    KEY_KP_ENTER: int
    KEY_KP_EQUAL: int

    KEY_BACK: int
    KEY_MENU: int
    KEY_VOLUME_UP: int
    KEY_VOLUME_DOWN: int


class Input:
    """Handles keyboard and mouse inputs from the engine."""
    
    @staticmethod
    def is_key_down(key: int) -> bool:
        """
        Checks if a key is currently being held down.
        Returns True every frame the key remains pressed.
        """
        ...
        
    @staticmethod
    def is_key_pressed(key: int) -> bool:
        """
        Checks if a key has been pressed once.
        Returns True only on the exact frame the key was pushed down.
        """
        ...

    @staticmethod
    def is_action_down(action: str) -> bool:
        """
        Checks if ANY of the keys bound to this named action are currently held down.
        Useful for continuous movement (e.g., Input.is_action_down("MoveRight")).
        """
        ...

    @staticmethod
    def is_action_pressed(action: str) -> bool:
        """
        Checks if ANY of the keys bound to this named action were pressed this frame.
        Useful for single actions like jumping or shooting (e.g., Input.is_action_pressed("Jump")).
        """
        ...


class SpriteRenderer(Component):
    """Renders a 2D texture at the position defined by the entity's Transform2d."""
    
    def __init__(self, texture_path: str) -> None: 
        """
        Loads an image file into GPU memory for rendering.
        
        Args:
            texture_path: The relative path to the image file (e.g., 'assets/player.png').
        """
        ...

    @property
    def width(self) -> float: ...

    @property
    def height(self) -> float: ...


class SpriteSheetRenderer(Component):
    """Renders a specific frame from a grid-based spritesheet texture."""
    
    def __init__(self, texture_path: str, columns: int, rows: int) -> None: 
        """
        Loads a spritesheet into GPU memory.
        
        Args:
            texture_path: The relative path to the image file.
            columns: The number of columns in the spritesheet grid.
            rows: The number of rows in the spritesheet grid.
        """
        ...
        
    @property
    def frame(self) -> int:
        """The current frame index being rendered (0 to frame_count - 1)."""
        ...
        
    @frame.setter
    def frame(self, value: int) -> None: ...
    
    @property
    def frame_count(self) -> int:
        """Total number of frames in the spritesheet (Read-only)."""
        ...


class ShapeRenderer(Component):
    """
    Component that renders a basic geometric rectangle.
    It can be drawn in world space or locked to the screen as a HUD element.
    """
    
    width: float
    """The base width of the shape in pixels (before Transform scale)."""
    
    height: float
    """The base height of the shape in pixels (before Transform scale)."""
    
    color: Color
    """The RGBA color of the shape."""
    
    is_hud: bool
    """
    If True, the shape is rendered in Screen Space (sticks to the camera viewport).
    If False, it is rendered in World Space.
    """
    
    def __init__(self) -> None:
        """
        Initializes a ShapeRenderer with default values (50x50, white).
        """
        ...


class Animation2d(Component):
    """
    Handles frame-by-frame animation by controlling a SpriteSheetRenderer.
    Performance is maximized as the timer logic runs natively in C++.
    """
    
    def __init__(self, frames: List[int], speed: float, loop: bool = True, flip_x: bool = False, flip_y: bool = False) -> None: 
        """
        Initializes the animation sequence.
        
        Args:
            frames: Sequence of frame indices to play (e.g., [0, 1, 2, 3]).
            speed: Time in seconds each frame is displayed.
            loop: If True, the animation repeats indefinitely.
        """
        ...


class LoopMode(Enum):
    """Defines how an animation should behave when it reaches the end."""
    Once = 0
    Loop = 1
    PingPong = 2


class Animator2d(Component):
    """
    Manages and plays 2D frame-based animations using an attached SpriteSheetRenderer.
    """

    def __init__(self) -> None:
        """Initializes an empty Animator2d."""
        ...

    @property
    def playback_speed(self) -> float:
        """The global speed multiplier for the animator (1.0 is normal speed)."""
        ...

    @playback_speed.setter
    def playback_speed(self, value: float) -> None:
        ...

    def add_animation(self, name: str, frames: list[int], frame_duration: float, loop: bool, flip_x: bool = False, flip_y: bool = False, events: dict[int, List[str]] = {}) -> None:
        """
        Registers a new animation state.
        
        :param name: The identifier for the animation (e.g., "walk", "idle").
        :param frames: A list of frame indices in the sprite sheet.
        :param frame_duration: Time in seconds each frame is displayed.
        :param loop: If True, the animation restarts automatically.
        """
        ...

    def play(self, name: str) -> None:
        """
        Switches the current playback to the specified animation.
        Does nothing if the animation is already playing.
        """
        ...

    def pause(self) -> None:
        """Pauses the current animation. It can be resumed later."""
        ...

    def resume(self) -> None:
        """Resumes a paused animation from where it left off."""
        ...

    def stop(self) -> None:
        """Stops the animation and resets it to the first frame."""
        ...

    def is_playing(self) -> bool:
        """Returns True if an animation is currently playing and not paused/stopped."""
        ...

    def is_finished(self) -> bool:
        """
        Returns True if the current non-looping animation has reached its final frame,
        or if the animator is currently stopped.
        """
        ...


class RectCollider(Component):
    """
    Component that defines a 2D rectangular collision shape.
    It does not apply physics forces by itself, it only defines the boundary.
    """
    
    size: Vector2
    """The width and height of the collision box (default is 50x50)."""
    
    offset: Vector2
    """The local offset of the collider relative to the GameObject's center position."""
    
    is_trigger: bool
    """
    If true, this collider will not physically block or bounce off other objects.
    It will only be used to detect overlapping areas (e.g., coins, checkpoints, damage zones).
    """

    def __init__(self, width: float = 50.0, height: float = 50.0) -> None:
        """
        Initializes a RectCollider.
        :param width: The width of the collision bounding box.
        :param height: The height of the collision bounding box.
        """
        ...


class RigidBody2d(Component):
    """
    Component that puts the GameObject under the control of the physics engine.
    It requires a Collider component on the same GameObject to actually hit things.
    """
    
    velocity: Vector2
    """The current linear speed and direction of the object."""
    
    mass: float
    """The weight of the object. Heavier objects push lighter objects during collisions."""
    
    gravity_scale: float
    """
    Multiplier applied to the world's gravity. 
    1.0 is normal gravity, 0.0 means the object floats, negative values make it fall upwards.
    """
    
    is_kinematic: bool
    """
    If true, the physics engine will NOT apply forces, gravity, or push this object during collisions.
    Useful for moving platforms or objects controlled entirely by animations/scripts.
    """

    @property
    def is_grounded(self) -> bool: ...

    def __init__(self) -> None:
        """
        Initializes a RigidBody2d with default values (mass=1.0, gravity=1.0, non-kinematic).
        """
        ...


class Color:
    """Represents an RGBA color."""
    r: int
    g: int
    b: int
    a: int
    
    def __init__(self, r: int = 0, g: int = 0, b: int = 0, a: int = 255) -> None: ...
    def __repr__(self) -> str: ...


class Camera2dAnchor(Enum):
    """
    Defines the base screen position that the camera uses as its focal point.
    """

    TopLeft = 0
    """Anchors the camera to the top-left corner of the screen (0, 0)."""
    
    Center = 1
    """Anchors the camera to the exact center of the screen."""


class Camera2d(Component):
    """
    Component that controls the viewport for rendering the game world.
    The camera automatically looks at the Transform2d of the GameObject it is attached to.
    """
    
    zoom: float
    """The zoom level of the camera. 1.0 is default, > 1.0 zooms in, < 1.0 zooms out."""
    
    offset: Vector2
    """
    Custom pixel offset applied ON TOP of the anchor point.
    Useful for shifting the camera view (e.g., aiming slightly higher or looking ahead).
    """
    
    anchor: Camera2dAnchor
    """The base screen alignment (default is CameraAnchor.Center)."""
    
    is_main: bool
    """
    If True, the Engine will use this camera to render the final Game View.
    Make sure only one active camera is marked as main at a time.
    """

    background_color: Color
    """
    The color used to clear the background before rendering the scene.
    Modify this to dynamically change the atmosphere or sky color of the level.
    """

    def __init__(self) -> None:
        """
        Initializes a Camera2d component with default values 
        (zoom=1.0, offset=(0,0), anchor=CameraAnchor.Center, is_main=True).
        """
        ...

    def shake(self, intensity: float, duration: float):
        """Triggers a screen shake (intensity, duration) """
        ...


class TileLayer:
    """Represents a single layer of tiles within a TileMap."""
    
    name: str
    """The display name of the layer."""
    
    is_visible: bool
    """Determines if the layer is currently rendered on screen."""
    
    is_solid: bool
    """Determines if the layer generates collision boxes for the physics engine."""


class Tileset:
    """Represents a texture atlas loaded into a TileMap. Read-only."""
    
    @property
    def name(self) -> str:
        """The name of the tileset (usually the filename)."""
        ...
        
    @property
    def first_tile_id(self) -> int:
        """The starting Global ID for the first tile of this specific texture."""
        ...
        
    @property
    def tile_count(self) -> int:
        """The total number of tiles contained in this texture."""
        ...
        
    @property
    def columns(self) -> int:
        """The number of columns of tiles in this texture."""
        ...


class TileMap(Component):
    """
    Manages a grid-based tile map with multiple layers, multi-tileset support, 
    and physics collision. Useful for level design and procedural generation.
    """
    
    grid_width: int
    """The number of columns in the grid."""
    
    grid_height: int
    """The number of rows in the grid."""
    
    tile_spacing: int
    """The gap in pixels between each tile in the source textures."""

    def __init__(self) -> None:
        """Initializes a new TileMap with a default 'Background' layer."""
        ...

    def add_tileset(self, path: str) -> None:
        """
        Appends a new tileset texture from the specified file path.
        The tileset is assigned a specific range of Global IDs based on its size.
        """
        ...

    def clear_tilesets(self) -> None:
        """
        Clears all loaded tilesets and frees the associated texture memory.
        """
        ...

    def get_tilesets(self) -> List[Tileset]:
        """
        Returns a list of all currently loaded Tilesets. 
        Useful for retrieving the 'first_tile_id' ranges during procedural generation.
        """
        ...
        
    def resize(self, new_width: int, new_height: int) -> None:
        """
        Resizes the map grid while safely preserving existing tile data.
        """
        ...

    def add_layer(self, name: str) -> None:
        """
        Adds a new empty layer on top of the existing ones.
        """
        ...

    def get_tile(self, layer_index: int, x: int, y: int) -> int:
        """
        Returns the Global Tile ID at the given grid coordinates.
        Returns -1 if the tile is empty or if the coordinates are out of bounds.
        """
        ...

    def set_tile(self, layer_index: int, x: int, y: int, global_tile_id: int) -> None:
        """
        Sets the Global Tile ID at the given grid coordinates on a specific layer.
        Use -1 as the global_tile_id to erase a tile.
        """
        ...

    @overload
    def get_layer(self, index: int) -> Optional[TileLayer]:
        """
        Retrieves a layer by its numerical index (0 is the bottom layer).
        Returns None if the index is out of bounds.
        """
        ...

    @overload
    def get_layer(self, name: str) -> Optional[TileLayer]:
        """
        Retrieves a layer by its assigned name.
        Returns None if no layer with this name exists.
        """
        ...


class TextRenderer(Component):
    """
    Renders text either in the game world or attached to the screen (HUD).
    """
    
    text: str
    """The string of text to display. Supports line breaks (\\n)."""

    font_size: float
    """The size of the rendered text."""

    is_hud: bool
    """If True, renders in screen space (ignoring the camera). If False, renders in world space."""

    font_path: str
    """The path to the custom .ttf or .otf font file. Leave empty to use the default engine font."""

    def __init__(self) -> None:
        """Initializes a new TextRenderer."""
        ...
        

class SceneManager:
    """Handles loading and transitioning between different scenes/levels."""
    
    @staticmethod
    def load_scene(name: str) -> None:
        """
        Requests the engine to load a new scene at the start of the next frame.
        The name should not include the extension or path (e.g., 'main_menu', 'level_01').
        """
        ...


class Scene:
    """
    Represents the currently active game scene, containing all active GameObjects.
    Provides global access and search utilities.
    """

    @staticmethod
    def find_object_by_name(name: str) -> Optional[GameObject]:
        """
        Searches for a GameObject by its exact name across the entire active scene.
        This is a global search and will find the object regardless of its parent.
        
        Args:
            name (str): The exact name of the GameObject to search for.
            
        Returns:
            Optional[GameObject]: The found GameObject instance, or None if no object matches the name.
            
        Example:
            camera = Scene.find_object_by_name("Main Camera")
            if camera:
                print("Camera found!")
        """
        ...

    @staticmethod
    def find_objects_with_tag(target_tag: str) -> List[GameObject]:
        """
        Searches for all active GameObjects matching the given tag across the entire scene.
        This method is highly optimized in C++ for fast radar and collision queries.
        
        Args:
            target_tag (str): The tag to search for (e.g., "Enemy").
            
        Returns:
            List[GameObject]: A list of all active GameObjects that possess the requested tag.
                              Returns an empty list if no objects match.
            
        Example:
            enemies = Scene.find_objects_with_tag("Enemy")
            for enemy in enemies:
                enemy_transform = enemy.get_component(Transform2d)
                # Calculate distance...
        """
        ...


class Globals:
    """Access global game variables managed in the Editor."""
    @staticmethod
    def set_int(key: str, value: int) -> None: ...

    @staticmethod
    def get_int(key: str, default_val: int = 0) -> int: ...

    @staticmethod
    def set_float(key: str, value: float) -> None: ...

    @staticmethod
    def get_float(key: str, default_val: float = 0.0) -> float: ...

    @staticmethod
    def set_bool(key: str, value: int) -> None: ...

    @staticmethod
    def get_bool(key: str, default_val: bool = False) -> bool: ...

    @staticmethod
    def set_string(key: str, value: str) -> None: ...

    @staticmethod
    def get_string(key: str, default_val: str = "") -> str: ...


class AudioSource(Component):
    def load_sfx(self, name: str, path: str) -> None: ...
    def play_sfx(self, name: str) -> None: ...
    
    def load_music(self, path: str) -> None: ...
    def play_music(self) -> None: ...
    def stop_music(self) -> None: ...
    def set_music_volume(self, volume: float) -> None: ...


class RaycastHit:
    """Contains information about a raycast impact."""
    
    hit: bool
    """True if the ray hit a solid object or a TileMap."""
    
    collider: Optional[GameObject]
    """The object hit by the ray (can be None if nothing was hit)."""
    
    point: Tuple[float, float]
    """The exact (x, y) world coordinates of the impact point."""
    
    distance: float
    """The distance between the ray's origin and the impact point."""


class Physics:
    """Global physics engine."""
    
    @staticmethod
    def raycast(origin: Tuple[float, float], direction: Tuple[float, float], distance: float) -> RaycastHit:
        """
        Casts an invisible ray into the scene to detect collisions.
        
        Example: Physics.raycast((x, y), (0, 1), 100.0) casts a ray downwards.
        """
        ...


class ButtonState:
    """Represents the current interaction state of a Button component."""
    Normal: 'ButtonState'
    Hovered: 'ButtonState'
    Pressed: 'ButtonState'


class ButtonTransition(Enum):
    None_ = 0
    ColorTint = 1
    SpriteSwap = 2


class Button(Component):
    """
    Component that provides a clickable interaction area.
    If a ShapeRenderer is attached to the same GameObject, the Button will 
    automatically update its color based on the current interaction state.
    """
    
    width: float
    """The width of the clickable hitbox."""
    
    height: float
    """The height of the clickable hitbox."""
    
    is_hud: bool
    """
    Determines how mouse coordinates are calculated.
    Ensure this matches the 'is_hud' property of your visual renderer (Text/Shape).
    """

    transition: ButtonTransition
    
    normal_color: Color
    """Color applied to an attached ShapeRenderer when inactive."""
    
    hover_color: Color
    """Color applied to an attached ShapeRenderer when the mouse hovers over it."""
    
    pressed_color: Color
    """Color applied to an attached ShapeRenderer when the mouse button is held down."""

    def __init__(self) -> None:
        """
        Initializes a Button with default dimensions and UI colors.
        """
        ...
        
    def is_clicked(self) -> bool:
        """
        Checks if the button was successfully clicked.
        
        Returns:
            bool: True only on the exact frame the user releases the left mouse button 
                  while still hovering the button bounds.
        """
        ...
        
    def get_state(self) -> ButtonState:
        """
        Retrieves the current visual state of the button.
        
        Returns:
            ButtonState: The interaction state (Normal, Hovered, or Pressed).
        """
        ...


class ParticleSystem2d(Component):
    """
    A versatile 2D particle emitter for creating visual effects like fire, rain, and explosions.
    All properties can be modified in real-time from Python to dynamically animate the emitter.
    """
    
    is_emitting: bool
    """If True, the system continuously spawns particles according to the emission_rate."""
    
    emission_rate: float
    """Number of particles generated per second."""
    
    life_min: float
    """Minimum lifetime of a generated particle in seconds."""
    
    life_max: float
    """Maximum lifetime of a generated particle in seconds."""
    
    speed_min: float
    """Minimum initial speed of a particle."""
    
    speed_max: float
    """Maximum initial speed of a particle."""
    
    emission_angle: float
    """The main direction the particles are fired in degrees (-90.0 is straight up)."""
    
    angle_spread: float
    """The cone width in degrees where particles can randomly spawn around the emission_angle."""
    
    gravity: float
    """Downward force applied to particles over time. Use negative values to make them float up."""
    
    start_color: Color
    """The color of the particle when it is born."""
    
    end_color: Color
    """The color of the particle when it dies (useful for fading out with alpha)."""
    
    start_size: float
    """The size in pixels of the particle when it is born."""
    
    end_size: float
    """The size in pixels of the particle when it dies."""

    def __init__(self) -> None:
        """Initializes a new ParticleSystem2d with default values."""
        ...

    def emit_burst(self, count: int) -> None:
        """
        Instantly fires a specific number of particles, ignoring the is_emitting state.
        Perfect for instantaneous effects like explosions, hit sparks, or item collection.
        
        Args:
            count: The number of particles to spawn immediately.
        """
        ...


class ScriptableObject:
    """
    Base class for all data-driven assets in Foxvoid Engine.
    Inherit from this class to create custom data containers (e.g., Items, Stats, Quests).
    The engine will automatically inspect your custom properties and generate an ImGui interface.
    """
    
    asset_id: str
    """The unique string identifier used to load this asset from disk."""
    
    name: str
    """The display name of the asset."""
    
    def __init__(self) -> None: 
        """Initializes an empty ScriptableObject."""
        ...


class DataManager:
    """
    Global system responsible for loading, caching, and saving ScriptableObject assets.
    """

    @staticmethod
    def load_asset(filepath: str) -> Optional[ScriptableObject]:
        """
        Loads a .asset file from disk. If the asset has already been loaded, 
        it returns the cached instance to save memory and performance.
        
        Args:
            filepath: The relative path to the .asset file (e.g., 'assets/data/sword.asset').
            
        Returns:
            The fully instantiated Python object, or None if loading failed.
        """
        ...

    @staticmethod
    def save_asset(asset: ScriptableObject, filepath: str) -> None:
        """
        Serializes a ScriptableObject instance and saves it to a .asset JSON file.
        """
        ...
        
    @staticmethod
    def clear_cache() -> None:
        """
        Clears all loaded assets from memory. 
        Usually called by the engine during scene transitions.
        """
        ...


class RectTransform(Component):
    """
    Component responsible for UI layout, managing anchors, pivots, and screen-space coordinates.
    Replaces Transform2d for HUD elements.
    """
    
    # The width and height of the UI element in pixels
    size: Vector2
    
    # The pixel offset relative to the calculated anchor point
    position: Vector2
    
    # Normalized screen anchor point (0.0 to 1.0)
    # (0,0) is top-left, (1,1) is bottom-right, (0.5, 0.5) is center
    anchor: Vector2
    
    # Normalized pivot point on the element itself (0.0 to 1.0)
    # Determines which part of the element corresponds to the 'position'
    pivot: Vector2

    def __init__(self) -> None:
        """Initializes a new RectTransform with default centered values."""
        ...

    def get_screen_rect(self) -> Rectangle:
        """
        Calculates the absolute pixel coordinates of the element on the screen.
        
        :return: A Rectangle object containing (x, y, width, height) ready for Raylib drawing.
        """
        ...


class ImageRenderer(Component):
    """
    Renders an image/texture. Uses RectTransform if is_hud is True (Screen Space), 
    or Transform2d if is_hud is False (World Space).
    """
    
    # Color tint applied to the image
    color: Color
    
    # Determines if the image is drawn in UI space or world space
    is_hud: bool

    def __init__(self, texture_path: str = "") -> None:
        """Initializes the ImageRenderer, optionally loading a texture immediately."""
        ...

    def set_texture(self, path: str) -> None:
        """
        Updates the displayed texture from a file path.
        Example: self.image.set_texture("assets/textures/menu_bg.png")
        """
        ...


class VBoxContainer(Component):
    """
    Automatically aligns child UI elements vertically based on their RectTransforms.
    Perfect for lists, menus, and inventories.
    """
    
    # Space between each child element in pixels
    spacing: float
    
    # Inner margins at the top and bottom
    padding_top: float
    padding_bottom: float
    
    # Forces horizontal alignment of children (0.0 = Left, 0.5 = Center, 1.0 = Right)
    horizontal_alignment: float

    def __init__(self) -> None: ...


class HBoxContainer(Component):
    """
    Automatically aligns child UI elements horizontally based on their RectTransforms.
    Perfect for toolbars, dialog buttons, and side-by-side layouts.
    """
    
    # Space between each child element in pixels
    spacing: float
    
    # Inner margins at the left and right
    padding_left: float
    padding_right: float
    
    # Forces vertical alignment of children (0.0 = Top, 0.5 = Center, 1.0 = Bottom)
    vertical_alignment: float

    def __init__(self) -> None: ...


class Mask(Component):
    """
    Clips (hides) all child UI elements that render outside of 
    this GameObject's RectTransform bounds.
    """
    
    # Allows turning the clipping effect on or off
    is_active: bool

    def __init__(self) -> None: ...
        

class Checkbox(Component):
    """
    A toggleable UI element. Triggers on_gui_click() when pressed.
    """
    is_on: bool
    use_sprite: bool
    color_on: 'Color'
    color_off: 'Color'

    def __init__(self) -> None: ...


class Slider(Component):
    """
    An interactive UI element that allows selecting a numerical value 
    by dragging a handle along a track. Triggers on_gui_click() dynamically.
    """
    value: float
    min_value: float
    max_value: float
    
    track_color: 'Color'
    handle_color: 'Color'
    active_handle_color: 'Color'

    def __init__(self) -> None: ...


class TextInput(Component):
    """
    An interactive text field. Captures keyboard input when clicked.
    Triggers on_gui_click() when the user presses 'Enter'.
    """
    text: str
    max_length: int
    font_size: int
    spacing: float
    
    text_color: 'Color'
    bg_color: 'Color'
    focused_bg_color: 'Color'

    def __init__(self) -> None: ...
    def is_focused(self) -> bool: ...

    def set_font(self, path: str) -> None:
        """
        Updates the font used by the text input.
        Example: self.input.set_font("assets/fonts/arial.ttf")
        """
        ...


class PolygonCollider(Component):
    """
    A custom polygonal shape collider.
    Warning: The shape MUST be convex (no hollow parts/indentations),
    and the vertices should ideally be defined in a clockwise order.
    """
    local_vertices: list[Vector2]
    offset: Vector2
    is_trigger: bool

    def __init__(self) -> None: 
        """
        Initializes a PolygonCollider with a default triangle shape.
        """
        ...

class CircleCollider(Component):
    """
    A perfect circle collider.
    Ideal for characters and rolling objects to prevent
    snagging on TileMap corners.
    """
    radius: float
    offset: Vector2
    is_trigger: bool

    def __init__(self, radius: float = 25.0) -> None: 
        """
        Initializes a CircleCollider with a given radius.
        """
        ...


class CapsuleCollider(Component):
    """
    A capsule-shaped collider, consisting of a cylinder with hemispherical ends.
    This is the industry standard shape for 2D character controllers as it slides
    smoothly over small bumps and does not catch on tile edges.
    """
    radius: float
    height: float
    offset: Vector2
    is_trigger: bool

    def __init__(self, radius: float = 25.0, height: float = 50.0) -> None: 
        """
        Initializes a CapsuleCollider. The height is the total vertical span
        and cannot be strictly smaller than radius * 2.
        """
        ...


class EventBus:
    """
    Global event bus for decoupled communication between C++ and Python, 
    or between different Python scripts. Uses JSON-serializable payloads.
    """

    @staticmethod
    def subscribe(event_name: str, callback: Callable[[Dict[str, Any]], None]) -> int:
        """
        Subscribes a callback function to a specific event.

        :param event_name: The name of the event to listen to.
        :param callback: A function that takes a dictionary (the parsed JSON payload) as its single argument.
        :return: A unique listener ID, which must be used to unsubscribe later.
        """
        ...

    @staticmethod
    def unsubscribe(event_name: str, listener_id: int) -> None:
        """
        Removes a specific listener from an event to prevent memory leaks and ghost calls.
        Should typically be called in the on_destroy() method of a ScriptComponent.

        :param event_name: The name of the event.
        :param listener_id: The unique ID returned by subscribe().
        """
        ...

    @staticmethod
    def publish(event_name: str, payload: Dict[str, Any]) -> None:
        """
        Publishes an event to all subscribers with a data payload.

        :param event_name: The name of the event.
        :param payload: A dictionary representing the data to send. It must be fully JSON serializable.
        """
        ...

    @staticmethod
    def publish_empty(event_name: str) -> None:
        """
        Publishes an event to all subscribers without a specific payload.
        Note: The subscribed callbacks will receive an empty dictionary {}.

        :param event_name: The name of the event.
        """
        ...
