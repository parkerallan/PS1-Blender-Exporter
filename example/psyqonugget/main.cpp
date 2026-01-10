/*
 * PlayStation 1 Rika Model Viewer - PSYQo
 * Complete model with ALL 200 triangles, ALL 264 quads, and animation
 */

#include "psyqo/application.hh"
#include "psyqo/fixed-point.hh"
#include "psyqo/font.hh"
#include "psyqo/fragments.hh"
#include "psyqo/gpu.hh"
#include "psyqo/gte-kernels.hh"
#include "psyqo/gte-registers.hh"
#include "psyqo/ordering-table.hh"
#include "psyqo/primitives/quads.hh"
#include "psyqo/primitives/triangles.hh"
#include "psyqo/scene.hh"
#include "psyqo/simplepad.hh"
#include "psyqo/soft-math.hh"
#include "psyqo/trigonometry.hh"
#include "psyqo/vector.hh"

#include <EASTL/array.h>

// Include the complete rika model and animation data
//.\png2tim-main\png2tim.exe -p 320 0 rikatexture.png;
//python bin2header.py rikatexture.tim rikatexture.h rikatexture_tim

#include "rika.h"
#include "rika-idle.h"
#include "rika-walk.h"
#include "rikatexture.h"

using namespace psyqo::fixed_point_literals;
using namespace psyqo::trig_literals;

static constexpr unsigned SCREEN_WIDTH = 320;
static constexpr unsigned SCREEN_HEIGHT = 240;
static constexpr unsigned ORDERING_TABLE_SIZE = 4096;

namespace {

class ModelViewer final : public psyqo::Application {
    void prepare() override;
    void createScene() override;
  public:
    psyqo::Trig<> m_trig;
    psyqo::SimplePad m_pad;
    psyqo::Font<> m_font;
};

class ModelViewerScene final : public psyqo::Scene {
    void start(StartReason reason) override;
    void frame() override;

    psyqo::Angle m_rotX = 0.0_pi;
    psyqo::Angle m_rotY = 0.0_pi;
    int32_t m_translateZ = 3000;
    int32_t m_translateY = -1000;
    
    unsigned m_frameCount = 0;
    unsigned m_animFrame = 0;
    bool m_animate = true;
    bool m_isWalking = false;

    psyqo::OrderingTable<ORDERING_TABLE_SIZE> m_ots[2];
    psyqo::Fragments::SimpleFragment<psyqo::Prim::FastFill> m_clear[2];
    
    // ALL faces - 200 triangles and 264 quads
    eastl::array<psyqo::Fragments::SimpleFragment<psyqo::Prim::TexturedTriangle>, 200> m_tris[2];
    eastl::array<psyqo::Fragments::SimpleFragment<psyqo::Prim::TexturedQuad>, 264> m_quads[2];

    bool m_textureUploaded = false;
    static constexpr psyqo::Color c_bg = {.r = 32, .g = 32, .b = 64};
    static constexpr psyqo::Color c_model = {.r = 128, .g = 128, .b = 128};
};

ModelViewer modelViewer;
ModelViewerScene modelViewerScene;

}

// Helper to get vertex from current animation frame
static inline psyqo::Vec3 getAnimVertex(unsigned vtxIdx, unsigned animFrame, bool isWalking) {
    psyqo::Vec3 v;
    if (isWalking) {
        if (animFrame < WALK_FRAMES_COUNT) {
            v.x.value = walk_anim[animFrame][vtxIdx].vx;
            v.y.value = walk_anim[animFrame][vtxIdx].vy;
            v.z.value = walk_anim[animFrame][vtxIdx].vz;
        } else {
            v.x.value = vertices[vtxIdx].vx;
            v.y.value = vertices[vtxIdx].vy;
            v.z.value = vertices[vtxIdx].vz;
        }
    } else {
        if (animFrame < IDLE_FRAMES_COUNT) {
            v.x.value = idle_anim[animFrame][vtxIdx].vx;
            v.y.value = idle_anim[animFrame][vtxIdx].vy;
            v.z.value = idle_anim[animFrame][vtxIdx].vz;
        } else {
            v.x.value = vertices[vtxIdx].vx;
            v.y.value = vertices[vtxIdx].vy;
            v.z.value = vertices[vtxIdx].vz;
        }
    }
    return v;
}

void ModelViewer::prepare() {
    psyqo::GPU::Configuration config;
    config.set(psyqo::GPU::Resolution::W320)
        .set(psyqo::GPU::VideoMode::AUTO)
        .set(psyqo::GPU::ColorMode::C15BITS)
        .set(psyqo::GPU::Interlace::PROGRESSIVE);
    gpu().initialize(config);
}

void ModelViewer::createScene() {
    m_pad.initialize();
    pushScene(&modelViewerScene);
}

void ModelViewerScene::start(StartReason reason) {
    psyqo::GTE::clear<psyqo::GTE::Register::TRX, psyqo::GTE::Unsafe>();
    psyqo::GTE::clear<psyqo::GTE::Register::TRY, psyqo::GTE::Unsafe>();
    psyqo::GTE::clear<psyqo::GTE::Register::TRZ, psyqo::GTE::Unsafe>();

    psyqo::GTE::write<psyqo::GTE::Register::OFX, psyqo::GTE::Unsafe>(
        psyqo::FixedPoint<16>(SCREEN_WIDTH / 2.0).raw());
    psyqo::GTE::write<psyqo::GTE::Register::OFY, psyqo::GTE::Unsafe>(
        psyqo::FixedPoint<16>(SCREEN_HEIGHT / 2.0).raw());

    psyqo::GTE::write<psyqo::GTE::Register::H, psyqo::GTE::Unsafe>(SCREEN_HEIGHT);
    psyqo::GTE::write<psyqo::GTE::Register::ZSF3, psyqo::GTE::Unsafe>(ORDERING_TABLE_SIZE / 6);
    psyqo::GTE::write<psyqo::GTE::Register::ZSF4, psyqo::GTE::Unsafe>(ORDERING_TABLE_SIZE / 8);

    // Upload font to VRAM
    modelViewer.m_font.uploadSystemFont(modelViewer.gpu());

    // Upload texture to VRAM (TIM file at 320, 0 with 256x256 16-bit texture)
    if (!m_textureUploaded) {
        const unsigned char* timData = rikatexture_tim;
        
        // 16-bit direct color texture - no CLUT needed, image data starts at offset 20
        const uint16_t* imageData = reinterpret_cast<const uint16_t*>(&timData[20]);
        psyqo::Rect texRect;
        texRect.pos.x = 320;
        texRect.pos.y = 0;
        texRect.size.w = 256;  // 256 pixels for 16-bit mode
        texRect.size.h = 256;
        modelViewer.gpu().uploadToVRAM(imageData, texRect);
        m_textureUploaded = true;
    }
}

void ModelViewerScene::frame() {
    auto& pad = modelViewer.m_pad;

    // Input handling
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Up)) m_rotX -= 0.02_pi;
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Down)) m_rotX += 0.02_pi;
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Left)) m_rotY -= 0.02_pi;
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Right)) m_rotY += 0.02_pi;
    
    // L2/R2 for zoom
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::L2)) {
        m_translateZ += 100;
        if (m_translateZ > 10000) m_translateZ = 10000;
    }
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::R2)) {
        m_translateZ -= 100;
        if (m_translateZ < 1000) m_translateZ = 1000;
    }
    
    // L1/R1 for height
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::L1)) m_translateY += 100;
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::R1)) m_translateY -= 100;
    
    if (pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Start)) m_animate = !m_animate;
    
    // Triangle to toggle animation
    static bool triangleWasPressed = false;
    bool trianglePressed = pad.isButtonPressed(psyqo::SimplePad::Pad1, psyqo::SimplePad::Triangle);
    if (trianglePressed && !triangleWasPressed) {
        m_isWalking = !m_isWalking;
        m_animFrame = 0;
    }
    triangleWasPressed = trianglePressed;

    int parity = gpu().getParity();
    auto& ot = m_ots[parity];
    auto& clear = m_clear[parity];

    ot.clear();
    gpu().getNextClear(clear.primitive, c_bg);
    gpu().chain(clear);

    // Update animation - 30fps (every other frame at 60fps)
    m_frameCount++;
    if (m_animate && m_frameCount % 2 == 0) {
        unsigned maxFrames = m_isWalking ? WALK_FRAMES_COUNT : IDLE_FRAMES_COUNT;
        m_animFrame = (m_animFrame + 1) % maxFrames;
    }

    // Set up GTE transformation
    psyqo::GTE::write<psyqo::GTE::Register::TRX, psyqo::GTE::Unsafe>(static_cast<uint32_t>(0));
    psyqo::GTE::write<psyqo::GTE::Register::TRY, psyqo::GTE::Unsafe>(static_cast<uint32_t>(m_translateY));
    psyqo::GTE::write<psyqo::GTE::Register::TRZ, psyqo::GTE::Unsafe>(static_cast<uint32_t>(m_translateZ));

    auto transform = psyqo::SoftMath::generateRotationMatrix33(m_rotX, psyqo::SoftMath::Axis::X, modelViewer.m_trig);
    auto rot = psyqo::SoftMath::generateRotationMatrix33(m_rotY, psyqo::SoftMath::Axis::Y, modelViewer.m_trig);
    psyqo::SoftMath::multiplyMatrix33(transform, rot, &transform);
    psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::Rotation>(transform);

    eastl::array<psyqo::Vertex, 4> projected;

    // Setup texture page - 16-bit direct color texture at (320, 0)
    psyqo::PrimPieces::TPageAttr tpage;
    tpage.setPageX(5).setPageY(0).set(psyqo::Prim::TPageAttr::ColorMode::Tex16Bits);

    // Render ALL 200 triangles
    for (unsigned i = 0; i < 200; i++) {
        auto v0 = getAnimVertex(tri_faces[i][0], m_animFrame, m_isWalking);
        auto v1 = getAnimVertex(tri_faces[i][1], m_animFrame, m_isWalking);
        auto v2 = getAnimVertex(tri_faces[i][2], m_animFrame, m_isWalking);

        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V0>(v0);
        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V1>(v1);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V2>(v2);
        psyqo::GTE::Kernels::rtpt();

        psyqo::GTE::Kernels::nclip();
        int32_t mac0 = 0;
        psyqo::GTE::read<psyqo::GTE::Register::MAC0>(reinterpret_cast<uint32_t*>(&mac0));
        if (mac0 <= 0) continue;

        psyqo::GTE::Kernels::avsz3();
        int32_t zIndex = 0;
        psyqo::GTE::read<psyqo::GTE::Register::OTZ>(reinterpret_cast<uint32_t*>(&zIndex));
        if (zIndex < 0 || zIndex >= static_cast<int32_t>(ORDERING_TABLE_SIZE)) continue;

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[0].packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&projected[1].packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&projected[2].packed);

        auto& tri = m_tris[parity][i];
        tri.primitive.pointA = projected[0];
        tri.primitive.pointB = projected[1];
        tri.primitive.pointC = projected[2];
        tri.primitive.setColor(c_model);
        tri.primitive.setOpaque();
        tri.primitive.tpage = tpage;
        
        tri.primitive.uvA.u = static_cast<uint8_t>(uvs[tri_uvs[i][0]].vx);
        tri.primitive.uvA.v = static_cast<uint8_t>(uvs[tri_uvs[i][0]].vy);
        tri.primitive.uvB.u = static_cast<uint8_t>(uvs[tri_uvs[i][1]].vx);
        tri.primitive.uvB.v = static_cast<uint8_t>(uvs[tri_uvs[i][1]].vy);
        tri.primitive.uvC.u = static_cast<uint8_t>(uvs[tri_uvs[i][2]].vx);
        tri.primitive.uvC.v = static_cast<uint8_t>(uvs[tri_uvs[i][2]].vy);
        
        ot.insert(tri, zIndex);
    }

    // Render ALL 264 quads
    for (unsigned i = 0; i < 264; i++) {
        auto v0 = getAnimVertex(quad_faces[i][0], m_animFrame, m_isWalking);
        auto v1 = getAnimVertex(quad_faces[i][1], m_animFrame, m_isWalking);
        auto v2 = getAnimVertex(quad_faces[i][2], m_animFrame, m_isWalking);
        auto v3 = getAnimVertex(quad_faces[i][3], m_animFrame, m_isWalking);

        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V0>(v0);
        psyqo::GTE::writeUnsafe<psyqo::GTE::PseudoRegister::V1>(v1);
        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V2>(v2);
        psyqo::GTE::Kernels::rtpt();

        psyqo::GTE::Kernels::nclip();
        int32_t mac0 = 0;
        psyqo::GTE::read<psyqo::GTE::Register::MAC0>(reinterpret_cast<uint32_t*>(&mac0));
        if (mac0 <= 0) continue;

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[0].packed);

        psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(v3);
        psyqo::GTE::Kernels::rtps();

        psyqo::GTE::Kernels::avsz4();
        int32_t zIndex = 0;
        psyqo::GTE::read<psyqo::GTE::Register::OTZ>(reinterpret_cast<uint32_t*>(&zIndex));
        if (zIndex < 0 || zIndex >= static_cast<int32_t>(ORDERING_TABLE_SIZE)) continue;

        psyqo::GTE::read<psyqo::GTE::Register::SXY0>(&projected[1].packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY1>(&projected[2].packed);
        psyqo::GTE::read<psyqo::GTE::Register::SXY2>(&projected[3].packed);

        auto& quad = m_quads[parity][i];
        quad.primitive.pointA = projected[0];
        quad.primitive.pointB = projected[1];
        quad.primitive.pointC = projected[2];
        quad.primitive.pointD = projected[3];
        quad.primitive.setColor(c_model);
        quad.primitive.setOpaque();
        quad.primitive.tpage = tpage;
        
        quad.primitive.uvA.u = static_cast<uint8_t>(uvs[quad_uvs[i][0]].vx);
        quad.primitive.uvA.v = static_cast<uint8_t>(uvs[quad_uvs[i][0]].vy);
        quad.primitive.uvB.u = static_cast<uint8_t>(uvs[quad_uvs[i][1]].vx);
        quad.primitive.uvB.v = static_cast<uint8_t>(uvs[quad_uvs[i][1]].vy);
        quad.primitive.uvC.u = static_cast<uint8_t>(uvs[quad_uvs[i][2]].vx);
        quad.primitive.uvC.v = static_cast<uint8_t>(uvs[quad_uvs[i][2]].vy);
        quad.primitive.uvD.u = static_cast<uint8_t>(uvs[quad_uvs[i][3]].vx);
        quad.primitive.uvD.v = static_cast<uint8_t>(uvs[quad_uvs[i][3]].vy);
        
        ot.insert(quad, zIndex);
    }

    gpu().chain(ot);
    
    // Display current animation
    const char* animName = m_isWalking ? "WALK" : "IDLE";
    psyqo::Color textColor = {.r = 255, .g = 255, .b = 255};
    modelViewer.m_font.print(gpu(), animName, {{.x = 10, .y = 10}}, textColor);
}

int main() {
    return modelViewer.run();
}
