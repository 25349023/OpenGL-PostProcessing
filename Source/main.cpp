#include "../Externals/Include/Common.h"

using namespace glm;
using namespace std;


enum MainMenuEntry
{
    MENU_RESET_POS = 1,
    MENU_SWITCH_RGB_NORMAL,
    // MENU_POST,
    MENU_NONE,
    MENU_IMAGE_ABSTRACTION,
    MENU_WATERCOLOR,
    MENU_EXIT
};

enum PostProcessing
{
    POST_NONE,
    POST_IMAGE_ABSTRACTION,
    POST_WATERCOLOR
};

enum DragMode
{
    DRAG_NONE,
    DRAG_CAMERA_EYE,
    DRAG_COMP_BAR
};

GLubyte timer_cnt = 0;
bool timer_enabled = true;
unsigned int timer_speed = 16;

const aiScene* scene;

GLuint program, program2;
const GLint mv_loc = 0, proj_loc = 1, tex_loc = 2;
const GLint fbtex_loc = 0, mode_loc = 1, wsize_loc = 2, cbar_loc = 3, ntex_loc = 4;
GLuint fvao, window_vbo, fbo, fbo_tex, normal_tex, depthrbo, active_ftex;

const int noise_size = 600;
float noise_map[noise_size][noise_size][3]{};
GLuint noise_tex;

int post_mode = POST_NONE;

mat4 projection, view, model;
vec3 eye(0, 0, 5), view_direction(0, 0, -1), up(0, 1, 0);
vec3 eye_x(-1, 0, 0), eye_y(0, 1, 0), eye_z(0, 0, -1);

int drag_mode;
ivec2 last_drag;


ivec2 win_size(600, 600);
int comparison_bar = 300;

struct Vertex
{
    vec3 position{};
    vec3 normal{};
    vec2 tex_coords{};
};

struct Shape
{
    GLuint vao;
    GLuint vbo;
    GLuint ibo;

    vector<Vertex> vertices;
    vector<unsigned int> indices;

    unsigned int draw_count;
    unsigned int material_id;

    Shape(): vao(0), vbo(0), ibo(0),
             draw_count(0), material_id(0) {}

    void extractMeshData(const aiMesh* mesh);
    void extractMeshIndices(const aiMesh* mesh);
    void bindBuffers();
};

void Shape::extractMeshData(const aiMesh* mesh)
{
    for (unsigned int v = 0; v < mesh->mNumVertices; ++v)
    {
        Vertex vertex;
        vertex.position = vec3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
        vertex.normal = vec3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);
        if (mesh->HasTextureCoords(0))
        {
            vertex.tex_coords = vec2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y);
        }
        vertices.push_back(vertex);
    }
}

void Shape::extractMeshIndices(const aiMesh* mesh)
{
    for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
    {
        unsigned int* idx = mesh->mFaces[f].mIndices;
        indices.insert(indices.end(), idx, idx + 3);
    }
}

void Shape::bindBuffers()
{
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // vertex buffer
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tex_coords));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    // index buffer
    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * indices.size(), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
}

struct Material
{
    GLuint diffuse_tex{};

    void bindTexture(texture_data texture);
};

void Material::bindTexture(texture_data texture)
{
    glGenTextures(1, &diffuse_tex);
    glBindTexture(GL_TEXTURE_2D, diffuse_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texture.width, texture.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, texture.data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

vector<Shape> shapes;
vector<Material> materials;

char** loadShaderSource(const char* file)
{
    FILE* fp = fopen(file, "rb");
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char* src = new char[sz + 1];
    fread(src, sizeof(char), sz, fp);
    src[sz] = '\0';
    char** srcp = new char*[1];
    srcp[0] = src;
    return srcp;
}

void freeShaderSource(char** srcp)
{
    delete[] srcp[0];
    delete[] srcp;
}

void loadGeometry()
{
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
    {
        const aiMesh* mesh = scene->mMeshes[i];
        Shape shape;

        shape.extractMeshData(mesh);
        shape.extractMeshIndices(mesh);

        shape.bindBuffers();

        shape.material_id = mesh->mMaterialIndex;
        shape.draw_count = mesh->mNumFaces * 3;

        shapes.push_back(shape);
    }
}

void loadMaterials()
{
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
    {
        aiMaterial* ai_material = scene->mMaterials[i];
        Material material;
        aiString texture_path;
        if (ai_material->GetTexture(aiTextureType_DIFFUSE, 0, &texture_path) == aiReturn_SUCCESS)
        {
            char full_path[64] = "../Objects/sponza/";
            strncat(full_path, texture_path.C_Str(), 50);

            texture_data texture = loadImg(full_path);
            material.bindTexture(texture);
        }

        materials.push_back(material);
    }
}

GLuint importShader(const char* path, GLenum shader_type)
{
    GLuint shader = glCreateShader(shader_type);
    char** shaderSource = loadShaderSource(path);
    glShaderSource(shader, 1, shaderSource, NULL);
    freeShaderSource(shaderSource);
    glCompileShader(shader);
    shaderLog(shader);

    return shader;
}

GLuint compileProgram(const char* vs_path, const char* fs_path)
{
    GLuint vertexShader = importShader(vs_path, GL_VERTEX_SHADER);
    GLuint fragmentShader = importShader(fs_path, GL_FRAGMENT_SHADER);

    GLuint pg = glCreateProgram();
    glAttachShader(pg, vertexShader);
    glAttachShader(pg, fragmentShader);
    glLinkProgram(pg);

    return pg;
}

void setModelMatrix()
{
    mat4 T(1), R(1), S(1);
    T = translate(T, vec3(0, -20, -50));
    R = mat4_cast(quat(vec3(0, radians(-90.0f), 0)));
    S = scale(S, vec3(0.5));
    model = T * R * S;
}

void updateViewMatrix()
{
    view = lookAt(eye, eye + view_direction, up);
}

struct Noise3
{
    static int i, j, k;
    static int A[3], T[8];

    static float u, v, w;

    static float noise(float x, float y, float z)
    {
        float s = (x + y + z) / 3;
        i = (int)floor(x + s);
        j = (int)floor(y + s);
        k = (int)floor(z + s);
        s = (i + j + k) / 6.;
        u = x - i + s;
        v = y - j + s;
        w = z - k + s;
        A[0] = A[1] = A[2] = 0;
        int hi = u >= w ? u >= v ? 0 : 1 : v >= w ? 1 : 2;
        int lo = u < w ? u < v ? 0 : 1 : v < w ? 1 : 2;
        return kernel(hi) + kernel(3 - hi - lo) + kernel(lo) + kernel(0);
    }

    static float kernel(int a)
    {
        float s = (A[0] + A[1] + A[2]) / 6.;
        float x = u - A[0] + s, y = v - A[1] + s, z = w - A[2] + s,
              t = .6f - x * x - y * y - z * z;
        int h = shuffle(i + A[0], j + A[1], k + A[2]);
        A[a]++;
        if (t < 0)
            return 0;
        int b5 = h >> 5 & 1, b4 = h >> 4 & 1, b3 = h >> 3 & 1, b2 = h >> 2 & 1, b = h & 3;
        float p = b == 1 ? x : b == 2 ? y : z,
              q = b == 1 ? y : b == 2 ? z : x,
              r = b == 1 ? z : b == 2 ? x : y;
        p = (b5 == b3 ? -p : p);
        q = (b5 == b4 ? -q : q);
        r = (b5 != (b4 ^ b3) ? -r : r);
        t *= t;
        return 8 * t * t * (p + (b == 0 ? q + r : b2 == 0 ? q : r));
    }

    static int shuffle(int i, int j, int k)
    {
        return b(i, j, k, 0) + b(j, k, i, 1) + b(k, i, j, 2) + b(i, j, k, 3) +
            b(j, k, i, 4) + b(k, i, j, 5) + b(i, j, k, 6) + b(j, k, i, 7);
    }

    static int b(int i, int j, int k, int B)
    {
        return T[b(i, B) << 2 | b(j, B) << 1 | b(k, B)];
    }

    static int b(int N, int B)
    {
        return N >> B & 1;
    }
};

int Noise3::i, Noise3::j, Noise3::k;
float Noise3::u, Noise3::v, Noise3::w;
int Noise3::A[3] = {0, 0, 0};
int Noise3::T[8] = {0x15, 0x38, 0x32, 0x2c, 0x0d, 0x13, 0x07, 0x2a};

void setupNoiseTexture()
{
    for (int i = 0; i < noise_size; i++)
    {
        for (int j = 0; j < noise_size; ++j)
        {
            for (int k = 0; k < 3; ++k)
            {
                noise_map[i][j][k] = Noise3::noise(i / 10.0f, j / 10.0f, k);
            }
        }
    }

    glGenTextures(1, &noise_tex);
    glBindTexture(GL_TEXTURE_2D, noise_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, noise_size, noise_size, 0,
                 GL_RGB, GL_FLOAT, noise_map);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void genFramebufferTexture(GLuint& tex, int attachment)
{
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 win_size.x, win_size.y, 0, GL_RGBA,GL_UNSIGNED_BYTE, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + attachment, GL_TEXTURE_2D, tex, 0);
}

void setupFrameBuffer()
{
    // vao for framebuffer shader
    glGenVertexArrays(1, &fvao);
    glBindVertexArray(fvao);

    const float window_vertex[] =
    {
        //vec2 position vec2 texture_coord
        1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
        -1.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f
    };

    glGenBuffers(1, &window_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, window_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(window_vertex), window_vertex, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, (void*)(sizeof(float) * 2));

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    // setup framebuffer
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Create fboDataTexture
    genFramebufferTexture(fbo_tex, 0);
    genFramebufferTexture(normal_tex, 1);
    active_ftex = fbo_tex;

    // Create Depth RBO
    glGenRenderbuffers(1, &depthrbo);
    glBindRenderbuffer(GL_RENDERBUFFER, depthrbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32, win_size.x, win_size.y);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthrbo);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(0);
}

void My_Init()
{
    glClearColor(0.8f, 0.9f, 1.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    scene = aiImportFile(
        "../Objects/sponza/sponza.obj",
        aiProcess_CalcTangentSpace |
        aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_JoinIdenticalVertices
    );

    cout << "There are " << scene->mNumMeshes << " meshes." << endl;
    cout << "There are " << scene->mNumMaterials << " materials." << endl;

    loadGeometry();
    loadMaterials();

    aiReleaseImport(scene);

    program = compileProgram("vertex.vs.glsl", "fragment.fs.glsl");
    program2 = compileProgram("framebuf.vs.glsl", "framebuf.fs.glsl");
    glUseProgram(program);

    setModelMatrix();
    updateViewMatrix();

    setupNoiseTexture();
    setupFrameBuffer();
}

void My_Display()
{
    const GLenum draw_buffers[] =
    {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1
    };

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glDrawBuffers(2, draw_buffers);

    glUseProgram(program);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    updateViewMatrix();

    mat4 mv = view * model;
    glUniformMatrix4fv(mv_loc, 1, GL_FALSE, value_ptr(mv));
    glUniformMatrix4fv(proj_loc, 1, GL_FALSE, value_ptr(projection));

    glActiveTexture(GL_TEXTURE0);
    glUniform1i(tex_loc, 0);

    for (const auto& shape : shapes)
    {
        glBindVertexArray(shape.vao);
        unsigned int material_id = shape.material_id;
        glBindTexture(GL_TEXTURE_2D, materials[material_id].diffuse_tex);
        glDrawElements(GL_TRIANGLES, shape.draw_count, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    // switch to screen framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(program2);
    glUniform1i(fbtex_loc, 0);
    glUniform1i(ntex_loc, 1);
    glUniform1i(mode_loc, post_mode);
    glUniform2f(wsize_loc, (float)win_size.x, (float)win_size.y);
    glUniform1i(cbar_loc, comparison_bar);

    glBindVertexArray(fvao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, active_ftex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, noise_tex);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);

    glutSwapBuffers();
}

void My_Reshape(int width, int height)
{
    glViewport(0, 0, width, height);
    win_size = vec2(width, height);
    comparison_bar = width / 2;

    float viewportAspect = (float)width / (float)height;
    projection = perspective(radians(60.0f), viewportAspect, 0.1f, 2000.0f);
    setupFrameBuffer();
}

void My_Timer(int val)
{
    glutPostRedisplay();
    glutTimerFunc(timer_speed, My_Timer, val);
}

void My_Mouse(int button, int state, int x, int y)
{
    if (state == GLUT_DOWN)
    {
        if (comparison_bar - 10 < x && x < comparison_bar + 10 && post_mode != POST_NONE)
        {
            drag_mode = DRAG_COMP_BAR;
        }
        else
        {
            drag_mode = DRAG_CAMERA_EYE;
        }
        last_drag = vec2(x, y);
    }
    else if (state == GLUT_UP)
    {
        drag_mode = DRAG_NONE;
    }
}

void My_Motion(int x, int y)
{
    switch (drag_mode)
    {
    case DRAG_NONE:
        break;
    case DRAG_CAMERA_EYE:
        {
            vec2 change = 0.2f * vec2(last_drag - ivec2(x, y));

            int sign = (view_direction.z > 0) ? -1 : 1;
            mat4 R = mat4_cast(quat(vec3(radians(sign * change.y), radians(change.x), 0)));

            view_direction = (R * vec4(view_direction, 1)).xyz;
            eye_z = (R * vec4(eye_z, 1)).xyz;
            eye_x = normalize(cross(up, eye_z));
            eye_y = normalize(cross(eye_z, eye_x));

            last_drag = vec2(x, y);
            break;
        }
    case DRAG_COMP_BAR:
        comparison_bar += x - last_drag.x;
        comparison_bar = std::max(0, std::min(comparison_bar, win_size.x));
        last_drag = vec2(x, y);
        break;
    default:
        break;
    }
}

void My_Keyboard(unsigned char key, int x, int y)
{
    const float speed = 5.0f;
    switch (key)
    {
    case 'w':
        eye += speed * eye_z;
        break;
    case 's':
        eye -= speed * eye_z;
        break;
    case 'a':
        eye += speed * eye_x;
        break;
    case 'd':
        eye -= speed * eye_x;
        break;
    case 'z':
        eye += speed * eye_y;
        break;
    case 'x':
        eye -= speed * eye_y;
        break;
    default:
        break;
    }
}

void My_SpecialKeys(int key, int x, int y)
{
    switch (key)
    {
    case GLUT_KEY_F1:
        printf("F1 is pressed at (%d, %d)\n", x, y);
        break;
    case GLUT_KEY_PAGE_UP:
        printf("Page up is pressed at (%d, %d)\n", x, y);
        break;
    case GLUT_KEY_LEFT:
        printf("Left arrow is pressed at (%d, %d)\n", x, y);
        break;
    default:
        printf("Other special key is pressed at (%d, %d)\n", x, y);
        break;
    }
}

void My_Menu(int id)
{
    switch (id)
    {
    case MENU_RESET_POS:
        eye = vec3(0, 0, 5);
        view_direction = vec3(0, 0, -1);
        up = vec3(0, 1, 0);
        eye_x = vec3(-1, 0, 0);
        eye_y = vec3(0, 1, 0);
        eye_z = vec3(0, 0, -1);
        break;
    case MENU_SWITCH_RGB_NORMAL:
        active_ftex = (active_ftex == fbo_tex) ? normal_tex : fbo_tex;
        break;
    case MENU_NONE:
        post_mode = POST_NONE;
        break;
    case MENU_IMAGE_ABSTRACTION:
        post_mode = POST_IMAGE_ABSTRACTION;
        break;
    case MENU_WATERCOLOR:
        post_mode = POST_WATERCOLOR;
        break;
    case MENU_EXIT:
        exit(0);
        break;
    default:
        break;
    }
}

int main(int argc, char* argv[])
{
#ifdef __APPLE__
    // Change working directory to source code path
    chdir(__FILEPATH__("/../Assets/"));
#endif
    // Initialize GLUT and GLEW, then create a window.
    ////////////////////
    glutInit(&argc, argv);
#ifdef _MSC_VER
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#else
    glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#endif
    // for RenderDoc debugging
    // glutInitContextVersion(4, 6);
    // glutInitContextProfile(GLUT_CORE_PROFILE);
    glutInitWindowPosition(100, 100);
    glutInitWindowSize(600, 600);
    glutCreateWindow("111062566_AS2");
    // You cannot use OpenGL functions before this line; The OpenGL context must be created first by glutCreateWindow()!
#ifdef _MSC_VER
    glewInit();
#endif
    dumpInfo();
    My_Init();

    // Create a menu and bind it to mouse right button.
    int menu_main = glutCreateMenu(My_Menu);
    int menu_post = glutCreateMenu(My_Menu);

    glutSetMenu(menu_main);
    glutAddMenuEntry("Reset Camera Position", MENU_RESET_POS);
    glutAddMenuEntry("Switch Diffuse / Normal", MENU_SWITCH_RGB_NORMAL);
    glutAddSubMenu("Post Processing", menu_post);
    glutAddMenuEntry("Exit", MENU_EXIT);

    glutSetMenu(menu_post);
    glutAddMenuEntry("None", MENU_NONE);
    glutAddMenuEntry("Image Abstraction", MENU_IMAGE_ABSTRACTION);
    glutAddMenuEntry("Watercolor", MENU_WATERCOLOR);

    glutSetMenu(menu_main);
    glutAttachMenu(GLUT_RIGHT_BUTTON);

    // Register GLUT callback functions.
    glutDisplayFunc(My_Display);
    glutReshapeFunc(My_Reshape);
    glutMouseFunc(My_Mouse);
    glutMotionFunc(My_Motion);
    glutKeyboardFunc(My_Keyboard);
    glutSpecialFunc(My_SpecialKeys);
    glutTimerFunc(timer_speed, My_Timer, 0);

    // Enter main event loop.
    glutMainLoop();

    return 0;
}
