// src/StaticModel.cpp
#include "StaticModel.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <iostream>
#include <limits>
#include <string>
#include <cstring>
#include <functional>
// stb_image single-file loader
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static glm::vec3 aiVec3ToGlm(const aiVector3D &v) { return glm::vec3(v.x, v.y, v.z); }
static glm::vec2 aiVec2ToGlm(const aiVector3D &v) { return glm::vec2(v.x, v.y); }

StaticModel::StaticModel() {}
StaticModel::~StaticModel() { Cleanup(); }

// compute axis-aligned bbox of all meshes attached to node in node-local coordinates
void StaticModel::ComputeMeshAABBForNode(const aiNode *node, glm::vec3 &outMin, glm::vec3 &outMax) const
{
    // initialize
    outMin = glm::vec3(std::numeric_limits<float>::infinity());
    outMax = glm::vec3(-std::numeric_limits<float>::infinity());

    if (!scene)
        return;

    for (unsigned int mi = 0; mi < node->mNumMeshes; ++mi)
    {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[mi]];
        if (!mesh || mesh->mNumVertices == 0)
            continue;
        for (unsigned int v = 0; v < mesh->mNumVertices; ++v)
        {
            aiVector3D av = mesh->mVertices[v];
            glm::vec3 p(av.x, av.y, av.z); // mesh vertices are already in mesh-local / node-local space after Assimp import
            outMin = glm::min(outMin, p);
            outMax = glm::max(outMax, p);
        }
    }
    // If node had no meshes, outMin/outMax will be +/-inf
}

// Walk scene nodes and compute pivots for nodes that have geometry.
void StaticModel::ComputeNodePivots()
{
    if (!scene)
        return;

    // recursive lambda
    std::function<void(const aiNode *)> recur = [&](const aiNode *nd)
    {
        glm::vec3 mn, mx;
        // initialize to inf so ComputeMeshAABBForNode will change them if meshes exist
        mn = glm::vec3(std::numeric_limits<float>::infinity());
        mx = glm::vec3(-std::numeric_limits<float>::infinity());

        // Compute bbox for this node's meshes (if any)
        ComputeMeshAABBForNode(nd, mn, mx);

        if (mn.x <= mx.x) // valid bbox
        {
            // choose pivot: use top edge (max.y) and center x/z to rotate legs around top of mesh
            glm::vec3 localCenter = (mn + mx) * 0.5f;
            glm::vec3 pivot = glm::vec3(localCenter.x, mx.y, localCenter.z);
            std::string nm(nd->mName.C_Str());
            nodePivots[nm] = pivot;
            // debug:
            // std::cout << "Pivot for node '"<<nm<<"' = ("<<pivot.x<<","<<pivot.y<<","<<pivot.z<<")\n";
        }
        // recurse children
        for (unsigned int i = 0; i < nd->mNumChildren; ++i)
            recur(nd->mChildren[i]);
    };

    recur(scene->mRootNode);
}

void StaticModel::Cleanup()
{
    for (auto &m : meshes)
    {
        if (m.ebo)
            glDeleteBuffers(1, &m.ebo);
        if (m.vbo)
            glDeleteBuffers(1, &m.vbo);
        if (m.vao)
            glDeleteVertexArrays(1, &m.vao);
        if (m.diffuseTex)
            glDeleteTextures(1, &m.diffuseTex);
    }
    meshes.clear();
}

GLuint StaticModel::LoadTextureFromFile(const std::string &filename, bool &outHasAlpha, bool silent)
{
    outHasAlpha = false;
    int w, h, n;
    stbi_uc *data = stbi_load(filename.c_str(), &w, &h, &n, 4); // force 4 channels (RGBA)
    if (!data)
    {
        if (!silent)
            std::cerr << "stb_image failed to load: " << filename << " reason: " << stbi_failure_reason() << "\n";
        return 0;
    }
    // if original channels < 4, n may be < 4; but we forced load to 4 -> check alpha content
    outHasAlpha = false;
    for (int i = 0; i < w * h; ++i)
    {
        if (data[i * 4 + 3] < 250)
        {
            outHasAlpha = true;
            break;
        } // loose test
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB_ALPHA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // wrap repeat default
    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

bool StaticModel::LoadFromFile(const std::string &path)
{
    Cleanup();

    scene = importer.ReadFile(path,
                              aiProcess_Triangulate |
                                  aiProcess_GenSmoothNormals |
                                  aiProcess_FlipUVs |
                                  aiProcess_CalcTangentSpace |
                                  aiProcess_JoinIdenticalVertices |
                                  aiProcess_OptimizeMeshes);

    if (!scene || !scene->HasMeshes())
    {
        std::cerr << "StaticModel: failed to load " << path << " (" << importer.GetErrorString() << ")\n";
        return false;
    }

    // directory for relative texture paths
    size_t p = path.find_last_of("/\\");
    directory = (p == std::string::npos) ? "." : path.substr(0, p);

    // For each mesh, collect vertex/index data and material
    meshes.resize(scene->mNumMeshes);

    for (unsigned int m = 0; m < scene->mNumMeshes; ++m)
    {
        aiMesh *mesh = scene->mMeshes[m];
        std::vector<SimpleVertex> verts;
        std::vector<unsigned int> inds;
        verts.resize(mesh->mNumVertices);
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
        {
            verts[i].pos = aiVec3ToGlm(mesh->mVertices[i]);
            verts[i].normal = mesh->HasNormals() ? aiVec3ToGlm(mesh->mNormals[i]) : glm::vec3(0, 1, 0);
            if (mesh->mTextureCoords[0])
            {
                // std::cerr << "Debug: mesh->mTextureCoords[0] exists." << std::endl;
                verts[i].uv = aiVec2ToGlm(mesh->mTextureCoords[0][i]);
            }

            else
                verts[i].uv = glm::vec2(0.0f, 0.0f);
        }
        for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
        {
            const aiFace &face = mesh->mFaces[f];
            if (face.mNumIndices != 3)
                continue;
            inds.push_back(face.mIndices[0]);
            inds.push_back(face.mIndices[1]);
            inds.push_back(face.mIndices[2]);
        }

        // create GL buffers
        MeshRenderData &dst = meshes[m];
        dst.indexCount = static_cast<GLsizei>(inds.size());

        glGenVertexArrays(1, &dst.vao);
        glGenBuffers(1, &dst.vbo);
        glGenBuffers(1, &dst.ebo);

        glBindVertexArray(dst.vao);
        glBindBuffer(GL_ARRAY_BUFFER, dst.vbo);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(SimpleVertex), verts.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dst.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size() * sizeof(unsigned int), inds.data(), GL_STATIC_DRAW);

        // attribs: location 0 = pos, 1 = normal, 2 = uv
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SimpleVertex), (void *)offsetof(SimpleVertex, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SimpleVertex), (void *)offsetof(SimpleVertex, normal));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SimpleVertex), (void *)offsetof(SimpleVertex, uv));

        glBindVertexArray(0);

        // material handling
        dst.hasDiffuse = false;
        dst.hasAlpha = false;
        dst.isHair = false;
        dst.diffuseTex = 0;
        dst.diffuseColor = glm::vec3(1.0f);

        if (scene->mNumMaterials > 0 && mesh->mMaterialIndex < scene->mNumMaterials)
        {
            aiMaterial *mat = scene->mMaterials[mesh->mMaterialIndex];

            // diffuse color (fallback)
            aiColor3D col(1.0f, 1.0f, 1.0f);
            if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_DIFFUSE, col))
            {
                dst.diffuseColor = glm::vec3(col.r, col.g, col.b);
            }
            // diffuse texture
            if (mat->GetTextureCount(aiTextureType_DIFFUSE) > 0)
            {
                aiString texPath;
                mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);
                // std::cout << "StaticModel: found diffuse texture path: " << texPath.C_Str() << "\n";
                std::string texFile = texPath.C_Str();

                // Skip embedded textures (GLB files use *0, *1, etc. as placeholders)
                if (texFile.empty() || texFile[0] == '*')
                {
                    std::cout << "StaticModel: skipping embedded texture placeholder: " << texFile << "\n";
                }
                else
                {
                    std::string full = texFile;

                    // Check if it's an absolute path (works on both Windows and Unix/Mac)
                    // Unix/Mac absolute path: starts with / (check this first, works on all platforms)
                    // Windows absolute path: C:\ or D:\ etc. (or C:/ or D:/)
                    bool isAbsolute = false;
                    if (!texFile.empty())
                    {
                        // Unix/Mac absolute path: starts with /
                        if (texFile[0] == '/')
                            isAbsolute = true;
// Windows absolute path: C:\ or D:\ etc.
#ifdef _WIN32
                        else if (texFile.length() >= 3 && texFile[1] == ':' && (texFile[2] == '\\' || texFile[2] == '/'))
                            isAbsolute = true;
#endif
                    }

                    if (isAbsolute)
                    {
                        // Extract filename from absolute path
                        size_t lastSlash = texFile.find_last_of("/\\");
                        std::string filename = (lastSlash == std::string::npos) ? texFile : texFile.substr(lastSlash + 1);

                        // Helper function to normalize path separators
                        auto normalizePath = [](const std::string &path) -> std::string
                        {
                            std::string result = path;
#ifdef _WIN32
                            // On Windows, replace / with \ for consistency
                            for (size_t i = 0; i < result.length(); ++i)
                            {
                                if (result[i] == '/')
                                    result[i] = '\\';
                            }
#else
                            // On Unix/Mac, replace \ with / for consistency
                            for (size_t i = 0; i < result.length(); ++i)
                            {
                                if (result[i] == '\\')
                                    result[i] = '/';
                            }
#endif
                            return result;
                        };

// Try to find texture in multiple locations
// 1. First try in model directory (same directory as .obj file)
#ifdef _WIN32
                        full = directory + "\\" + filename;
#else
                        full = directory + "/" + filename;
#endif
                        full = normalizePath(full);
                        dst.diffuseTex = LoadTextureFromFile(full, dst.hasAlpha, true); // silent for first attempt

                        if (!dst.diffuseTex)
                        {
                            // 2. If not found, try in blender directory (common case)
                            // Find project root by looking for "opengl" in directory path
                            size_t openglPos = directory.find("opengl");
                            if (openglPos != std::string::npos)
                            {
                                std::string projectRoot = directory.substr(0, openglPos);
#ifdef _WIN32
                                full = projectRoot + "blender\\textures\\" + filename;
#else
                                full = projectRoot + "blender/textures/" + filename;
#endif
                                full = normalizePath(full);
                                dst.diffuseTex = LoadTextureFromFile(full, dst.hasAlpha, false); // show errors for final attempt
                            }

                            if (!dst.diffuseTex)
                            {
                                // 3. Try in assets/models directory
                                size_t assetsPos = directory.find("assets");
                                if (assetsPos != std::string::npos)
                                {
                                    std::string baseDir = directory.substr(0, assetsPos);
#ifdef _WIN32
                                    full = baseDir + "assets\\models\\" + filename;
#else
                                    full = baseDir + "assets/models/" + filename;
#endif
                                    full = normalizePath(full);
                                    dst.diffuseTex = LoadTextureFromFile(full, dst.hasAlpha, false); // show errors for final attempt
                                }
                            }
                        }
                    }
                    else
                    {
                        // Relative path: make absolute relative to model directory
                        if (texFile.find_first_of("/\\") == std::string::npos)
                            full = directory + "/" + texFile;
                        else
                            full = directory + "/" + texFile;
                        dst.diffuseTex = LoadTextureFromFile(full, dst.hasAlpha, false);
                    }

                    if (dst.diffuseTex)
                    {
                        // std::cout << "StaticModel: loaded diffuse texture " << full << "\n";
                        dst.hasDiffuse = true;
                    }
                    else
                    {
                        std::cerr << "StaticModel: failed to load diffuse texture " << full << "\n";
                    }
                }
            }

            // opacity or transparency detection
            float opacity = 1.0f;
            if (AI_SUCCESS == aiGetMaterialFloat(mat, AI_MATKEY_OPACITY, &opacity))
            {
                if (opacity < 0.999f)
                    dst.hasAlpha = true;
            }

            // heuristic: if material name or texture filename contains "hair" or "fur", mark as hair
            aiString matName;
            if (AI_SUCCESS == mat->Get(AI_MATKEY_NAME, matName))
            {
                std::string name = matName.C_Str();
                for (auto &c : name)
                    c = tolower(c);
                if (name.find("hair") != std::string::npos || name.find("fur") != std::string::npos)
                {
                    dst.isHair = true;
                    dst.alphaCutoff = 0.4f;
                }
            }
            if (!dst.isHair && dst.hasDiffuse)
            {
                // also check texture filename
                std::string t = "";
                if (mat->GetTextureCount(aiTextureType_DIFFUSE) > 0)
                {
                    aiString tpath;
                    mat->GetTexture(aiTextureType_DIFFUSE, 0, &tpath);
                    t = tpath.C_Str();
                    for (auto &c : t)
                        c = tolower(c);
                    if (t.find("hair") != std::string::npos || t.find("fur") != std::string::npos)
                    {
                        dst.isHair = true;
                        dst.alphaCutoff = 0.4f;
                    }
                }
            }
        }
    }
    // After assimp import:
    // this->scene = scene; // however you store it
    nodePivots.clear();
    ComputeNodePivots();

    bboxInitialized = false;
    ComputeBBoxRecursive(scene->mRootNode, scene, glm::mat4(1.0f));
    // std::cout << "StaticModel: loaded meshes=" << meshes.size() << " from " << path << std::endl;
    return true;
}

void StaticModel::Draw(GLuint shaderProgram) const
{
    // we assume shaderProgram is already in use, and uniforms uHasDiffuse, uHasAlpha, uUseAlphaTest,
    // uAlphaCutoff, uMatDiffuse and sampler2D uDiffuseMap exist.
    GLint locHasDiffuse = glGetUniformLocation(shaderProgram, "uHasDiffuse");
    GLint locHasAlpha = glGetUniformLocation(shaderProgram, "uHasAlpha");
    GLint locUseAlphaTest = glGetUniformLocation(shaderProgram, "uUseAlphaTest");
    GLint locAlphaCutoff = glGetUniformLocation(shaderProgram, "uAlphaCutoff");
    GLint locMatDiffuse = glGetUniformLocation(shaderProgram, "uMatDiffuse");
    GLint locDiffuseMap = glGetUniformLocation(shaderProgram, "uDiffuseMap");

    for (const auto &m : meshes)
    {
        // set diffuse color
        if (locMatDiffuse >= 0)
            glUniform3f(locMatDiffuse, m.diffuseColor.r, m.diffuseColor.g, m.diffuseColor.b);

        // texture binding
        if (m.hasDiffuse && m.diffuseTex)
        {
            if (locHasDiffuse >= 0)
                glUniform1i(locHasDiffuse, 1);
            if (locDiffuseMap >= 0)
            {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, m.diffuseTex);
                glUniform1i(locDiffuseMap, 0);
            }
        }
        else
        {
            if (locHasDiffuse >= 0)
                glUniform1i(locHasDiffuse, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        // alpha/hair handling
        if (locHasAlpha >= 0)
            glUniform1i(locHasAlpha, m.hasAlpha ? 1 : 0);
        if (locUseAlphaTest >= 0)
            glUniform1i(locUseAlphaTest, (m.hasAlpha || m.isHair) ? 1 : 0);
        if (locAlphaCutoff >= 0)
            glUniform1f(locAlphaCutoff, m.alphaCutoff);

        // blending for hair: enable blending if isHair
        if (m.isHair)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            // also disable depth write optionally to reduce artifacts:
            glDepthMask(GL_FALSE);
        }

        // draw mesh
        glBindVertexArray(m.vao);
        glDrawElements(GL_TRIANGLES, m.indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        // restore state
        if (m.isHair)
        {
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }
    }

    // unbind texture
    glBindTexture(GL_TEXTURE_2D, 0);
}

static glm::mat4 aiMatToGlm(const aiMatrix4x4 &m)
{
    return glm::mat4(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4);
}

void StaticModel::ComputeBBoxRecursive(
    aiNode *node,
    const aiScene *scene,
    const glm::mat4 &parentTransform)
{
    glm::mat4 nodeTransform = parentTransform * aiMatToGlm(node->mTransformation);

    // 遍历该 node 挂载的 mesh
    for (unsigned int i = 0; i < node->mNumMeshes; ++i)
    {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
        for (unsigned int v = 0; v < mesh->mNumVertices; ++v)
        {
            glm::vec3 p = aiVec3ToGlm(mesh->mVertices[v]);
            glm::vec4 worldP = nodeTransform * glm::vec4(p, 1.0f);
            glm::vec3 wp(worldP);

            if (!bboxInitialized)
            {
                bboxMin = bboxMax = wp;
                bboxInitialized = true;
            }
            else
            {
                bboxMin = glm::min(bboxMin, wp);
                bboxMax = glm::max(bboxMax, wp);
            }
        }
    }

    // 递归子节点
    for (unsigned int c = 0; c < node->mNumChildren; ++c)
    {
        ComputeBBoxRecursive(node->mChildren[c], scene, nodeTransform);
    }
    // std::cout << "bboxMin = "
    //           << bboxMin.x << ", "
    //           << bboxMin.y << ", "
    //           << bboxMin.z << std::endl;

    // std::cout << "bboxMax = "
    //           << bboxMax.x << ", "
    //           << bboxMax.y << ", "
    //           << bboxMax.z << std::endl;
}

void StaticModel::DrawDepth() const
{
    for (const auto &m : meshes)
    {
        glBindVertexArray(m.vao);
        glDrawElements(GL_TRIANGLES, m.indexCount, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
}

// ---- Helper: adapt these to your MeshRenderData definition ----
// I will assume you have a member std::vector<MeshRenderData> meshes;
// and MeshRenderData contains at least: unsigned int VAO; unsigned int indexCount; unsigned int VBO (maybe);
// possibly unsigned int EBO; unsigned int diffuseTex; glm::vec3 diffuseColor; bool hasDiffuseTex;
// If your field names differ, change below accordingly.

void StaticModel::DrawMeshByIndex(unsigned int meshIndex, unsigned int shaderID) const
{
    if (!scene)
        return; // or handle accordingly
    if (meshIndex >= meshes.size())
    {
        std::cerr << "DrawMeshByIndex: invalid meshIndex " << meshIndex << std::endl;
        return;
    }

    const auto &m = meshes[meshIndex];

    // --- set material/texture uniforms (if present) ---
    GLint locHasDiffuse = glGetUniformLocation(shaderID, "uHasDiffuse");
    GLint locDiffuseMap = glGetUniformLocation(shaderID, "uDiffuseMap");
    GLint locMatDiffuse = glGetUniformLocation(shaderID, "uMatDiffuse");

    // Example field names — update to your actual struct:
    // m.diffuseTex   -> GLuint texture id (0 if none)
    // m.hasDiffuseTex -> bool
    // m.diffuseColor -> glm::vec3

    bool hasTex = false;
    GLuint texId = 0;
    glm::vec3 matColor(1.0f, 1.0f, 1.0f);

// Try to read common field names (adapt if your names differ)
// ======= ADAPT HERE if your struct fields are different =======
// Example assumptions:
// m.diffuseTex (GLuint), m.hasDiffuse (bool), m.diffuseColor (glm::vec3)
#ifdef HAS_MESH_FIELDS_EXPLICIT // remove this define in real code; it's explanatory
    hasTex = m.hasDiffuse;
    texId = m.diffuseTex;
    matColor = m.diffuseColor;
#else
// Try common variants with safe checks using sizeof/offsetof is overkill here;
// you should replace these lines with the actual fields from your MeshRenderData:
// e.g. hasTex = (m.diffuseTextureID != 0);
//      texId = m.diffuseTextureID;
//      matColor = m.baseColor;
#endif
    // --- end adapt section ---

    // If your mesh struct actually contains a texture id field named `texture_diffuse`:
    // uncomment/modify the below to match:
    // hasTex = (m.texture_diffuse != 0);
    // texId  = m.texture_diffuse;

    if (locHasDiffuse >= 0)
        glUniform1i(locHasDiffuse, hasTex ? 1 : 0);
    if (locMatDiffuse >= 0)
        glUniform3f(locMatDiffuse, matColor.r, matColor.g, matColor.b);
    if (hasTex && locDiffuseMap >= 0)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texId);
        glUniform1i(locDiffuseMap, 0);
    }
    else if (locDiffuseMap >= 0)
    {
        // ensure shader doesn't sample stale texture unit
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUniform1i(locDiffuseMap, 0);
    }

    // --- bind VAO and draw ---
    // Common struct fields: m.VAO, m.indexCount, m.hasIndices (or m.EBO)
    glBindVertexArray(m.vao);

    if (m.indexCount > 0)
    {
        // indexed draw: assume indices are uint
        glDrawElements(GL_TRIANGLES, (GLsizei)m.indexCount, GL_UNSIGNED_INT, 0);
    }
    else
    {
        // fallback: draw arrays using vertex count (if stored as vertexCount)
        // if (m.vertexCount > 0)
        //     glDrawArrays(GL_TRIANGLES, 0, (GLsizei)m.vertexCount);
        // else
        //     std::cerr << "DrawMeshByIndex: mesh " << meshIndex << " has no indices or vertexCount\n";
    }

    // unbind VAO and texture (optional hygiene)
    glBindVertexArray(0);
    if (hasTex)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void StaticModel::DrawNodeAnimated(const aiNode *nd, const glm::mat4 &parentTransform, unsigned int shaderID)
{
    // std::cout << "Drawing node " << nd << std::endl;
    // compute node transform
    glm::mat4 nodeTransform = parentTransform * aiMatToGlm(nd->mTransformation);

    // check node name for animation targets
    std::string nm(nd->mName.C_Str());

    glm::mat4 animatedTransform = nodeTransform; // default

    // set up animation for legs and tail: frequency, amplitude, phase
    float t = (float)glfwGetTime();
    float legFreq = 5.5f;
    float legAmp = glm::radians(13.0f);

    // simple name matching (case-sensitive). If your node names differ, adjust strings.
    if (nm.find("Leg") != std::string::npos || nm.find("leg") != std::string::npos)
    {
        // std::cout << "animEnable: " << (animEnable ? "true" : "false") << std::endl;
        if (!animEnable)
        {
            animatedTransform = nodeTransform; // 原样绘制，不动画
        }
        else
        {
            // determine phase by which leg
            float phase = 0.0f;
            if (nm.find("Front_L") != std::string::npos || nm.find("FrontLeft") != std::string::npos || nm.find("Front_L") != std::string::npos)
                phase = 0.0f;
            else if (nm.find("Front_R") != std::string::npos || nm.find("FrontRight") != std::string::npos)
                phase = glm::pi<float>();
            else if (nm.find("Back_L") != std::string::npos || nm.find("BackLeft") != std::string::npos)
                phase = glm::pi<float>();
            else if (nm.find("Back_R") != std::string::npos || nm.find("BackRight") != std::string::npos)
                phase = 0.0f;

            float rawAngle = sinf(t * legFreq + phase) * legAmp;
            float angle = rawAngle * animBlend;
            // pivot in local node coordinates (if available), otherwise use origin (0,0,0)
            glm::vec3 pivot(0.0f);
            auto it = nodePivots.find(nm);
            if (it != nodePivots.end())
                pivot = it->second;
            // glm::vec3 size = bboxMax - bboxMin;
            // glm::vec3 pivot = bboxMin;
            // pivot.y = bboxMax.y;

            // rotate around local X axis (forward/back swing). If your leg's local axis differs, change axis.
            glm::mat4 T1 = glm::translate(glm::mat4(1.0f), pivot);
            glm::mat4 R = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(1, 0, 0));
            glm::mat4 T2 = glm::translate(glm::mat4(1.0f), -pivot);

            // apply locally: nodeTransform is in model-space; we must insert local rotation
            animatedTransform = nodeTransform * T1 * R * T2;
        }
    }
    else if (nm == "Body")
    {
        float bob = 0.0f;
        if (animBlend > 0.001f)
            bob = sinf(t * legFreq * 0.5f) * 0.01f * animBlend; // 1cm 级别

        animatedTransform = glm::translate(nodeTransform, glm::vec3(0, bob, 0));
    }

    // Draw meshes attached to this node using animatedTransform as model
    // set uniform uModel/uNormalMat as in your normal draw path
    GLint locModel = glGetUniformLocation(shaderID, "uModel");
    if (locModel >= 0)
        glUniformMatrix4fv(locModel, 1, GL_FALSE, &animatedTransform[0][0]);

    GLint locNormal = glGetUniformLocation(shaderID, "uNormalMat");
    if (locNormal >= 0)
    {
        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(animatedTransform)));
        glUniformMatrix3fv(locNormal, 1, GL_FALSE, &normalMat[0][0]);
    }

    // draw each mesh of this node
    for (unsigned int i = 0; i < nd->mNumMeshes; ++i)
    {
        unsigned int meshIndex = nd->mMeshes[i];
        // your existing mesh draw routine; e.g.:
        // meshes[meshIndex].Draw(shaderID);
        DrawMeshByIndex(meshIndex, shaderID); // replace with your actual function
    }

    // recurse children with nodeTransform (or animatedTransform if you want children to follow)
    for (unsigned int c = 0; c < nd->mNumChildren; ++c)
    {
        DrawNodeAnimated(nd->mChildren[c], animatedTransform, shaderID);
        // Note: we pass nodeTransform to children if you don't want child's transform to be affected
        // by the local animation; if you DO want children to follow, pass animatedTransform instead.
    }
}
// 新接口：接收外部 modelMatrix
void StaticModel::DrawAnimated(const glm::mat4 &rootModel, float deltaTime, unsigned int shaderID)
{
    // 平滑逼近目标状态
    float target = animEnable ? 1.0f : 0.0f;
    float speed = 6.0f; // 越大，切换越快

    animBlend += (target - animBlend) * speed * deltaTime;
    animBlend = glm::clamp(animBlend, 0.0f, 1.0f);
    if (!scene)
        return;
    DrawNodeAnimated(scene->mRootNode, rootModel, shaderID);
}
