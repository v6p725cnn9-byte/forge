#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

namespace forge::game {

struct CourseBox {
    glm::vec3 center;
    glm::vec3 half;
    float pitch = 0;
    glm::vec4 color{0.27f, 0.34f, 0.40f, 1};
    glm::mat4 transform() const
    {
        return glm::translate(glm::mat4(1), center)
            * glm::rotate(glm::mat4(1), glm::radians(pitch), glm::vec3{1, 0, 0})
            * glm::scale(glm::mat4(1), half * 2.0f);
    }
};
struct CourseLabel { glm::vec3 position; const char* text; };

// Shared by rendering and Jolt: changing a tester cannot desynchronize its collider.
inline const std::vector<CourseBox>& locomotion_course()
{
    static const auto boxes = [] {
        std::vector<CourseBox> out;
        for (int lane = 0; lane < 3; ++lane) {
            const float rise = 0.15f + lane * 0.10f;
            for (int step = 0; step < 8; ++step) {
                const float h = (step + 1) * rise;
                out.push_back({{-12.0f - lane * 4.0f, h * 0.5f, -6.0f - step * 0.65f},
                               {1.4f, h * 0.5f, 0.325f}, 0,
                               step % 2 ? glm::vec4{.32f,.42f,.48f,1} : glm::vec4{.20f,.28f,.34f,1}});
            }
        }
        for (int lane = 0; lane < 3; ++lane) {
            const float angle = 15.0f + lane * 15.0f;
            const float y = 3.0f * std::sin(glm::radians(angle)) - 0.05f;
            out.push_back({{-12.0f - lane * 4.0f,y,-18.0f},{1.4f,.10f,3.0f}, angle,
                           {.22f,.42f,.34f,1}});
        }
        out.push_back({{-5, .4f, -10}, {1.4f,.4f,.6f}, 0, {.65f,.39f,.14f,1}});
        out.push_back({{-5, .8f, -15}, {1.4f,.8f,1.4f}, 0, {.65f,.39f,.14f,1}});
        out.push_back({{-5, 1.1f, -21}, {1.4f,1.1f,1.4f}, 0, {.65f,.39f,.14f,1}});
        // 0.8 m tunnel clearance: only a prone capsule fits.
        out.push_back({{2,1.0f,-10},{1.7f,.2f,2.5f}});
        out.push_back({{.1f,.6f,-10},{.2f,.6f,2.5f}});
        out.push_back({{3.9f,.6f,-10},{.2f,.6f,2.5f}});
        out.push_back({{2,1.55f,-18},{1.7f,.2f,2.0f}});
        out.push_back({{.1f,.85f,-18},{.2f,.85f,2}});
        out.push_back({{3.9f,.85f,-18},{.2f,.85f,2}});
        return out;
    }();
    return boxes;
}
inline const std::vector<CourseLabel>& course_labels()
{
    static const std::vector<CourseLabel> labels{
        {{-12,.8f,-4},"STAIRS 15 cm"}, {{-16,.8f,-4},"STAIRS 25 cm"},
        {{-20,.8f,-4},"STAIRS 35 cm"}, {{-12,1,-14},"SLOPE 15"},
        {{-16,1,-14},"SLOPE 30"}, {{-20,1,-14},"SLOPE 45 / SLIDE"},
        {{-5,1.6f,-9},"SPACE / LOW MANTLE"}, {{-5,2.5f,-15},"CLIMB 1.6 m"},
        {{-5,3,-21},"CLIMB / DROP 2.2 m"}, {{2,1.8f,-7},"C / CRAWL 0.8 m"},
        {{2,2.3f,-16},"CTRL / CROUCH 1.35 m"}};
    return labels;
}

} // namespace forge::game
