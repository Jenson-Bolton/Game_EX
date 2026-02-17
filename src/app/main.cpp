/**
 * @brief Main renderer responsible for drawing all world objects.
 *
 * Handles OpenGL state, shaders, and draw batching.
 * Must be initialized before use.
 */
class Renderer
{
public:
    /**
     * @brief Initializes the renderer.
     * @return True if initialization succeeded.
     */
    bool Init();

    /**
     * @brief Renders a frame.
     * @param deltaTime Time since last frame in seconds.
     */
    void Render(float deltaTime);
};
