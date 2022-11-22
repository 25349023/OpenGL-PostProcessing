#include "../Externals/Include/Common.h"

#define MENU_TIMER_START 1
#define MENU_TIMER_STOP 2
#define MENU_EXIT 3

GLubyte timer_cnt = 0;
bool timer_enabled = true;
unsigned int timer_speed = 16;

const aiScene* scene;

GLuint vao;

using namespace glm;
using namespace std;

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

    int drawCount;
    int materialID;

    Shape(): vao(0), vbo(0), ibo(0),
             drawCount(0), materialID(0) {}

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
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tex_coords));

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

        shape.materialID = mesh->mMaterialIndex;
        shape.drawCount = mesh->mNumFaces * 3;

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
            char full_path[64] = "../Objects/";
            strncat(full_path, texture_path.C_Str(), 50);
            
            texture_data texture = loadImg(full_path);
            material.bindTexture(texture);
        }
        // else
        // {
            // load some default image as default_diffuse_tex
            // Material.diffuse_tex = default_diffuse_tex;
        // }

        materials.push_back(material);
    }
}

void My_Init()
{
    glClearColor(0.0f, 0.6f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    scene = aiImportFile(
        "../Objects/sibenik.obj",
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_SortByPType
    );

    cout << "There are " << scene->mNumMeshes << " meshes." << endl;
    cout << "There are " << scene->mNumMaterials << " materials." << endl;

    loadGeometry();
    loadMaterials();

    aiReleaseImport(scene);
}

void My_Display()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glutSwapBuffers();
}

void My_Reshape(int width, int height)
{
    glViewport(0, 0, width, height);
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
        printf("Mouse %d is pressed at (%d, %d)\n", button, x, y);
    }
    else if (state == GLUT_UP)
    {
        printf("Mouse %d is released at (%d, %d)\n", button, x, y);
    }
}

void My_Keyboard(unsigned char key, int x, int y)
{
    printf("Key %c is pressed at (%d, %d)\n", key, x, y);
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
    case MENU_TIMER_START:
        if (!timer_enabled)
        {
            timer_enabled = true;
            glutTimerFunc(timer_speed, My_Timer, 0);
        }
        break;
    case MENU_TIMER_STOP:
        timer_enabled = false;
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
    int menu_timer = glutCreateMenu(My_Menu);

    glutSetMenu(menu_main);
    glutAddSubMenu("Timer", menu_timer);
    glutAddMenuEntry("Exit", MENU_EXIT);

    glutSetMenu(menu_timer);
    glutAddMenuEntry("Start", MENU_TIMER_START);
    glutAddMenuEntry("Stop", MENU_TIMER_STOP);

    glutSetMenu(menu_main);
    glutAttachMenu(GLUT_RIGHT_BUTTON);

    // Register GLUT callback functions.
    glutDisplayFunc(My_Display);
    glutReshapeFunc(My_Reshape);
    glutMouseFunc(My_Mouse);
    glutKeyboardFunc(My_Keyboard);
    glutSpecialFunc(My_SpecialKeys);
    glutTimerFunc(timer_speed, My_Timer, 0);

    // Enter main event loop.
    glutMainLoop();

    return 0;
}
