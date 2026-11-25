<--------------------CO1 & CO2------------------------>
Use of OpenGL primitives and simple drawing algorithms as per requirements.

2D drawing primitives:
- `frontend/opengl/UI/Renderer.cpp`
  - `rect()` uses `GL_QUADS`
  - `circle()` uses `GL_TRIANGLE_FAN`
  - `ring()` uses `GL_TRIANGLE_STRIP`
  - `line()` and `polyline()` use `GL_LINES` and `GL_LINE_STRIP`
  - `triangle()` uses `GL_TRIANGLES`
  - `hgradient()` and `vgradient()` use colored `GL_QUADS`
  - Lines: 184-293

- `frontend/opengl/Charts/Charts.cpp`
  - `barChart()`, `hbarChart()`, `donut()` and `lineChart()` draw 2D charts with the shared renderer and OpenGL primitives.
  - `box()` and `Chart3D::draw()` implement the interactive 3D chart.
  - Lines: 34-205 (2D charts) and 207-327 (3D chart)

- `frontend/opengl/main.cpp`
  - Creates the OpenGL window, enables multisampling and line smoothing.
  - Lines: 54-66

The complete application uses OpenGL for the dashboard, forms, map, reports and chart interfaces.


<-------------------CO3 & CO5-------------------->
1. Apply transformation and clipping
2. Manipulating 3D objects and handling dynamic graph viewports

Clipping:
- `frontend/opengl/UI/Renderer.cpp`
- `applyScissor()` converts renderer rectangles into an OpenGL scissor rectangle.
- `pushClip()` intersects nested clip areas and enables clipping.
- `popClip()` restores the parent clip area.
- Lines: 108-121 and 383-398

Clipping is used to keep rendered map, chart and panel contents inside their assigned regions.

Transformation:
- `frontend/opengl/Map/IndiaMap.cpp`
- `render()` applies `glTranslatef()` to move the map to the viewport and `glScalef()` to apply pan/zoom scaling.
- The map uses `vp_.x`, `vp_.y`, `offX_`, `offY_`, `baseScale_` and `zoom_`.
- Lines: 292-318

Viewport:
- `Renderer::beginFrame()` in `frontend/opengl/UI/Renderer.cpp`
- Configures `glViewport()` and the 2D orthographic projection for window/pixel-scale coordinates.
- Lines: 159-177

Manipulating 3D objects:
- `frontend/opengl/Charts/Charts.cpp`
- `Chart3D::draw()` uses a dedicated viewport and sets the perspective projection.
- `gluLookAt()` positions the camera.
- `glRotatef()` applies pitch and yaw rotation.
- `glTranslatef()` centers the chart.
- `glScalef()` scales the bar heights according to the data range.
- Lines: 234-327


<--------------------------CO6-------------------------->
Mapping:

1. Realistic Surface Textures:
- `frontend/opengl/UI/Renderer.cpp`
- The font renderer builds a 2048 x 2048 alpha texture atlas containing ASCII and extra Unicode glyphs.
- `glGenTextures()` creates the texture.
- `glBindTexture()` binds the generated font texture.
- `glTexImage2D()` uploads the glyph atlas to the GPU.
- Linear minification and magnification filters provide smooth text.
- Lines: 54-83

- `Renderer::text()` binds the texture atlas and draws each character using textured `GL_QUADS`.
- It uses `stbtt_GetPackedQuad()` and `glTexCoord2f()` to map each glyph from the texture atlas.
- Lines: 308-348

2. Lighting Effects:
- `frontend/opengl/Charts/Charts.cpp`
- `Chart3D::draw()` enables depth testing, `GL_LIGHTING`, `GL_LIGHT0`, color material and normal normalization.
- Ambient and diffuse light values and the light position are configured with `glLightfv()`.
- Lines: 273-291

3. Assembling the Complete Application Framework:
- `frontend/opengl/main.cpp`
- Initializes FreeGLUT/OpenGL, creates the window, registers callbacks and starts the application loop.
- Lines: 54-83
- `frontend/opengl/App/App.cpp`
- Connects the dashboard screens, navigation, database state and rendering system.
- `frontend/opengl/UI/Renderer.cpp`
- Provides shared 2D rendering and text services.


<---------------------------CO4--------------------------->
Finalize texture mappings, refine coordinate alignments, and verify transformation precision across all visual dashboard modules.

Related implementations:

1. Window and 2D coordinate setup:
- `Renderer::beginFrame()` sets the viewport and orthographic coordinate system.
- `frontend/opengl/UI/Renderer.cpp`, lines 159-177

2. Texture mapping:
- The font texture atlas is generated and sampled for text in `Renderer.cpp`.
- Glyph texture coordinates are mapped onto each character quad.
- Lines: 54-83 and 308-348

3. Map alignment and transformations:
- `IndiaMap::render()` translates the map origin and scales district coordinates to support zoom and pan.
- `frontend/opengl/Map/IndiaMap.cpp`, lines 292-318
- District outlines are drawn in screen space to maintain a consistent one-pixel width.
- Lines: 320-363

4. 3D chart transformations and viewport:
- The 3D chart uses a dedicated viewport, perspective projection, camera position, pitch/yaw rotation, translation and height scaling.
- `frontend/opengl/Charts/Charts.cpp`, lines 234-327
