/*
 * eos - A 3D Morphable Model fitting library written in modern C++11/14.
 *
 * File: eos-model-viewer.cpp
 *
 * Copyright 2017, 2018 Patrik Huber
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "cxxopts.hpp"

#include "eos/core/Mesh.hpp"
#include "eos/morphablemodel/MorphableModel.hpp"
#include "eos/morphablemodel/io/cvssp.hpp"
#include "eos/morphablemodel/Blendshape.hpp"
#include "eos/cpp17/variant.hpp"

#include "igl/opengl/glfw/Viewer.h"
#include "igl/opengl/glfw/imgui/ImGuiMenu.h"
#include "igl/opengl/glfw/imgui/ImGuiHelpers.h"
#include "imgui/imgui.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>

template <typename T>
std::string to_string(const T a_value, const int n = 6)
{
    std::ostringstream out;
    out << std::setprecision(n) << a_value;
    return out.str();
};

// From: https://stackoverflow.com/a/1493195/1345959
template <class ContainerType>
void tokenize(const std::string& str, ContainerType& tokens, const std::string& delimiters = " ",
              bool trim_empty = false)
{
    std::string::size_type pos, last_pos = 0;
    const auto length = str.length();

    using value_type = typename ContainerType::value_type;
    using size_type = typename ContainerType::size_type;

    while (last_pos < length + 1)
    {
        pos = str.find_first_of(delimiters, last_pos);
        if (pos == std::string::npos)
        {
            pos = length;
        }

        if (pos != last_pos || !trim_empty)
            tokens.push_back(value_type(str.data() + last_pos, (size_type)pos - last_pos));

        last_pos = pos + 1;
    }
};

eos::morphablemodel::MorphableModel load_bin_or_scm_model(std::string model_file)
{
    using namespace eos;
    using eos::morphablemodel::MorphableModel;

    MorphableModel morphable_model;

    std::vector<std::string> tokens;
    tokenize(model_file, tokens, ".");
    const auto model_file_extension = tokens.back();

    // Todo: Add try-catch to all these?
    if (model_file_extension == "scm")
    {
        morphable_model = morphablemodel::load_scm_model(model_file);
    } else if (model_file_extension == "bin")
    {
        morphable_model = morphablemodel::load_model(model_file);
    } else
    {
        throw std::runtime_error("Error: Please load a model with .bin or .scm extension.");
    }
    return morphable_model;
}

/**
 *
 */
eos::morphablemodel::MorphableModel load_model(std::string model_file, std::string blendshapes_file)
{
    using namespace eos;
    using eos::morphablemodel::MorphableModel;

    MorphableModel morphable_model;

    // Todo: Add try-catch to all these?
    morphable_model = load_bin_or_scm_model(model_file);
    // If separate blendshapes are given, load them, and construct a model with expressions:
    if (!blendshapes_file.empty())
    {
        const auto blendshapes = morphablemodel::load_blendshapes(blendshapes_file);
        morphable_model =
            MorphableModel(morphable_model.get_shape_model(), blendshapes, morphable_model.get_color_model(),
                           morphable_model.get_texture_coordinates());
    }
    return morphable_model;
};

/**
 * Model viewer for 3D Morphable Models.
 */
int main(int argc, const char* argv[])
{
    using namespace eos;
    using std::begin;
    using std::cout;
    using std::end;
    using std::endl;
    using std::for_each;
    using std::string;
    using std::vector;

    string model_file, blendshapes_file;
    try
    {
        cxxopts::Options options("eos-model-viewer", "OpenGL viewer for eos's 3D morphable models.");
        // clang-format off
        options.add_options()
            ("h,help", "display the help message")
            ("m,model", "an eos 3D Morphable Model stored as cereal BinaryArchive (.bin)",
                cxxopts::value(model_file))
            ("b,blendshapes", "an eos file with blendshapes (.bin)",
                cxxopts::value(blendshapes_file));
        // clang-format on
        const auto result = options.parse(argc, argv);
        if (result.count("help"))
        {
            cout << options.help() << endl;
            return EXIT_SUCCESS;
        }
    } catch (const cxxopts::OptionException& e)
    {
        cout << "Error parsing options: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    // Note: Take the vertices here directly, so we can maybe avoid ever generating a Mesh instance
    auto get_V = [](const core::Mesh& mesh) {
        Eigen::MatrixXd V(mesh.vertices.size(), 3);
        for (int i = 0; i < mesh.vertices.size(); ++i)
        {
            V(i, 0) = mesh.vertices[i](0);
            V(i, 1) = mesh.vertices[i](1);
            V(i, 2) = mesh.vertices[i](2);
        }
        return V;
    };
    auto get_F = [](const core::Mesh& mesh) {
        Eigen::MatrixXi F(mesh.tvi.size(), 3);
        for (int i = 0; i < mesh.tvi.size(); ++i)
        {
            F(i, 0) = mesh.tvi[i][0];
            F(i, 1) = mesh.tvi[i][1];
            F(i, 2) = mesh.tvi[i][2];
        }
        return F;
    };
    auto get_C = [](const core::Mesh& mesh) {
        Eigen::MatrixXd C(mesh.colors.size(), 3);
        for (int i = 0; i < mesh.colors.size(); ++i)
        {
            C(i, 0) = mesh.colors[i](0);
            C(i, 1) = mesh.colors[i](1);
            C(i, 2) = mesh.colors[i](2);
        }
        return C;
    };

    // Init the viewer:
    igl::opengl::glfw::Viewer viewer;

    // Attach a menu plugin:
    igl::opengl::glfw::imgui::ImGuiMenu menu;
    viewer.plugins.push_back(&menu);

    morphablemodel::MorphableModel morphable_model;
    // Load the model right away on start up, if it was given via command-line parameters:
    if (!model_file.empty())
    {
        try
        {
            // Loads .bin or .scm model, with or without blendshapes:
            morphable_model = load_model(model_file, blendshapes_file);
            const auto& mean = morphable_model.get_mean();
            viewer.data().set_mesh(get_V(mean), get_F(mean));
            viewer.core.align_camera_center(viewer.data().V, viewer.data().F);
            if (!mean.colors.empty())
            {
                viewer.data().set_colors(get_C(mean));
            }

        } catch (const std::runtime_error& e)
        {
            cout << "Error loading the given model: " << e.what() << endl;
            return EXIT_FAILURE;
        }
    }

    // These are the coefficients of the currently active mesh instance:
    vector<float> shape_coefficients;
    vector<float> color_coefficients;
    vector<float> expression_coefficients;

    std::default_random_engine rng;
    std::array<float, 3> random_sample_sdev = {1.0f, 1.0f, 1.0f}; // shp, exp, col

    // Draw our viewers windows:
    menu.callback_draw_custom_window = [&]() {
        // Load model & draw sample options:
        ImGui::SetNextWindowPos(ImVec2(0.f * menu.menu_scaling(), 585), ImGuiSetCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(240, 240), ImGuiSetCond_FirstUseEver);
        ImGui::Begin("Morphable Model", nullptr, ImGuiWindowFlags_NoSavedSettings);
        if (ImGui::Button("Load Morphable Model", ImVec2(-1, 0)))
        {
            const string mm_fn = igl::file_dialog_open();
            cout << "Loading Morphable Model " << mm_fn << "..." << endl;
            try
            {
                morphable_model = load_bin_or_scm_model(mm_fn);
                const auto& mean = morphable_model.get_mean();
                viewer.data().clear();
                viewer.data().set_mesh(get_V(mean), get_F(mean));
                viewer.core.align_camera_center(viewer.data().V, viewer.data().F);
                if (!mean.colors.empty())
                {
                    viewer.data().set_colors(get_C(mean));
                }
            } catch (const std::runtime_error&
                         e) // Todo: I think we have to catch more errors here, like cereal exceptions
            {
                cout << "Error loading the given model: " << e.what() << endl;
            }
        }
        if (ImGui::Button("Load Blendshapes", ImVec2(-1, 0)))
        {
            const string bs_fn = igl::file_dialog_open();
            cout << "Loading Blendshapes " << bs_fn << "..." << endl;
            morphablemodel::Blendshapes blendshapes;
            try
            {
                blendshapes = morphablemodel::load_blendshapes(bs_fn);
                cout << "Blendshapes loaded. Constructing a new model consisting of the loaded identity and "
                        "colour PCA models, and the loaded blendshapes..."
                     << endl;
                morphable_model = morphablemodel::MorphableModel(
                    morphable_model.get_shape_model(), blendshapes, morphable_model.get_color_model(),
                    morphable_model.get_texture_coordinates());
                const auto& mean = morphable_model.get_mean();
                viewer.data().clear();
                viewer.data().set_mesh(get_V(mean), get_F(mean));
                viewer.core.align_camera_center(viewer.data().V, viewer.data().F);
                if (!mean.colors.empty())
                {
                    viewer.data().set_colors(get_C(mean));
                }

            } catch (const std::runtime_error&
                         e) // Todo: I think we have to catch more errors here, like cereal exceptions
            {
                cout << "Error loading the given blendshapes: " << e.what() << endl;
            }
        }
        ImGui::Separator();
        if (ImGui::Button("Mean", ImVec2(-1, 0)))
        {
            const auto& mean = morphable_model.get_mean();
            viewer.data().set_vertices(get_V(mean));
            if (!mean.colors.empty())
            {
                viewer.data().set_colors(get_C(mean));
            }
            for_each(begin(shape_coefficients), end(shape_coefficients), [](auto& coeff) { coeff = 0.0f; });
            for_each(begin(color_coefficients), end(color_coefficients), [](auto& coeff) { coeff = 0.0f; });
            for_each(begin(expression_coefficients), end(expression_coefficients),
                     [](auto& coeff) { coeff = 0.0f; });
        }
        if (ImGui::Button("Random face sample", ImVec2(-1, 0)))
        {
            // Shape sample:
            std::normal_distribution<float> shape_coeffs_dist(0.0f,
                                                              random_sample_sdev[0]); // c'tor takes stddev
            shape_coefficients =
                vector<float>(morphable_model.get_shape_model().get_num_principal_components());
            for_each(begin(shape_coefficients), end(shape_coefficients),
                     [&rng, &shape_coeffs_dist](auto& coeff) { coeff = shape_coeffs_dist(rng); });
            // Expression sample:
            if (morphable_model.has_separate_expression_model())
            {

                if (eos::cpp17::holds_alternative<morphablemodel::Blendshapes>(
                        morphable_model.get_expression_model().value()))
                {
                    std::uniform_real_distribution<float> expression_coeffs_dist(
                        0.0f,
                        random_sample_sdev[1]); // using the interval [0, sdev] (so it's not really an sdev
                                                // here!)
                    const auto expression_model_num_coeffs =
                        eos::cpp17::get<morphablemodel::Blendshapes>(
                            morphable_model.get_expression_model().value())
                            .size();
                    expression_coefficients = vector<float>(expression_model_num_coeffs);
                    for_each(begin(expression_coefficients), end(expression_coefficients),
                             [&rng, &expression_coeffs_dist](auto& coeff) {
                                 coeff = expression_coeffs_dist(rng);
                             });

                } else if (eos::cpp17::holds_alternative<morphablemodel::PcaModel>(
                               morphable_model.get_expression_model().value()))
                {
                    std::normal_distribution<float> expression_coeffs_dist(0.0f, random_sample_sdev[1]);
                    const auto expression_model_num_coeffs =
                        eos::cpp17::get<morphablemodel::PcaModel>(
                            morphable_model.get_expression_model().value())
                            .get_num_principal_components();
                    expression_coefficients = vector<float>(expression_model_num_coeffs);
                    for_each(begin(expression_coefficients), end(expression_coefficients),
                             [&rng, &expression_coeffs_dist](auto& coeff) {
                                 coeff = expression_coeffs_dist(rng);
                             });
                }
            }
            // Colour sample:
            std::normal_distribution<float> color_coeffs_dist(0.0f, random_sample_sdev[2]);
            color_coefficients =
                vector<float>(morphable_model.get_color_model().get_num_principal_components());
            for_each(begin(color_coefficients), end(color_coefficients),
                     [&rng, &color_coeffs_dist](auto& coeff) { coeff = color_coeffs_dist(rng); });

            // Now generate the sample:
            core::Mesh sample;
            // Note: Can it happen that we pass in colour-coeffs, but it's a model without colour?
            if (morphable_model.has_separate_expression_model())
            {
                sample = morphable_model.draw_sample(shape_coefficients, expression_coefficients,
                                                     color_coefficients);
            } else
            {
                sample = morphable_model.draw_sample(shape_coefficients, color_coefficients);
            }
            viewer.data().set_vertices(get_V(sample));
            if (!sample.colors.empty())
            {
                viewer.data().set_colors(get_C(sample));
            }
        }
        if (ImGui::Button("Random identity sample", ImVec2(-1, 0)))
        {
        }
        if (ImGui::Button("Random expression sample", ImVec2(-1, 0)))
        {
        }
        if (ImGui::Button("Random color sample", ImVec2(-1, 0)))
        {
        }
        ImGui::InputFloat3("sdev [shp, exp, col]", &random_sample_sdev[0], 2);
        ImGui::End(); // end "Morphable Model" window

        // PCA shape coefficients:
        ImGui::SetNextWindowPos(ImVec2(180.f * menu.menu_scaling(), 0), ImGuiSetCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(200, 160), ImGuiSetCond_FirstUseEver);
        ImGui::Begin("Shape PCA", nullptr, ImGuiWindowFlags_NoSavedSettings);

        ImGui::Text("Coefficients");
        if (morphable_model.get_shape_model().get_num_principal_components() > 0) // a model was loaded
        {
            int num_shape_coeffs_to_display =
                std::min(morphable_model.get_shape_model().get_num_principal_components(), 30);

            if (shape_coefficients.empty() || shape_coefficients.size() != num_shape_coeffs_to_display)
            {
                // no coeffs yet, or number of coeffs has changed for some reason:
                shape_coefficients.resize(num_shape_coeffs_to_display);
            }
            // shape_coefficients are always initialised now.

            ImGui::BeginGroup();
            for (int i = 0; i < num_shape_coeffs_to_display; ++i)
            {
                const string label = std::to_string(i);
                ImGui::SliderFloat(label.c_str(), &shape_coefficients[i], -3.0, 3.0);
            }
            ImGui::EndGroup();
            string coeffs_displayed =
                "Displaying " + std::to_string(num_shape_coeffs_to_display) + "/" +
                std::to_string(morphable_model.get_shape_model().get_num_principal_components()) +
                " coefficients.";
            ImGui::Text(coeffs_displayed.c_str());
        }

        ImGui::End(); // end "Shape PCA" window

        // PCA colour coefficients:
        ImGui::SetNextWindowPos(ImVec2(380.f * menu.menu_scaling(), 0), ImGuiSetCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(200, 160), ImGuiSetCond_FirstUseEver);
        ImGui::Begin("Colour PCA", nullptr, ImGuiWindowFlags_NoSavedSettings);

        ImGui::Text("Coefficients");
        if (morphable_model.get_color_model().get_num_principal_components() > 0) // a model was loaded
        {
            int num_color_coeffs_to_display =
                std::min(morphable_model.get_color_model().get_num_principal_components(), 30);

            if (color_coefficients.empty() || color_coefficients.size() != num_color_coeffs_to_display)
            {
                color_coefficients.resize(num_color_coeffs_to_display);
            }
            // color_coefficients are always initialised now.

            ImGui::BeginGroup();
            for (int i = 0; i < num_color_coeffs_to_display; ++i)
            {
                const string label = std::to_string(i);
                ImGui::SliderFloat(label.c_str(), &color_coefficients[i], -3.0, 3.0);
            }
            ImGui::EndGroup();
            string coeffs_displayed =
                "Displaying " + std::to_string(num_color_coeffs_to_display) + "/" +
                std::to_string(morphable_model.get_color_model().get_num_principal_components()) +
                " coefficients.";
            ImGui::Text(coeffs_displayed.c_str());
        }

        ImGui::End(); // end "Colour PCA" window

        // PCA expression coefficients:
        ImGui::SetNextWindowPos(ImVec2(580.f * menu.menu_scaling(), 0), ImGuiSetCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(200, 160), ImGuiSetCond_FirstUseEver);
        ImGui::Begin("Expression PCA", nullptr, ImGuiWindowFlags_NoSavedSettings);

        ImGui::Text("Coefficients");
        if (morphable_model.has_separate_expression_model()) // a model was loaded
        {
            int expression_model_num_coeffs = 0;
            float slider_min_range = -3.0f;
            float slider_max_range = +3.0f;
            if (eos::cpp17::holds_alternative<morphablemodel::Blendshapes>(
                    morphable_model.get_expression_model().value()))
            {
                expression_model_num_coeffs = eos::cpp17::get<morphablemodel::Blendshapes>(
                                                  morphable_model.get_expression_model().value())
                                                  .size();
                slider_min_range = -1.0f;
            } else if (eos::cpp17::holds_alternative<morphablemodel::PcaModel>(
                           morphable_model.get_expression_model().value()))
            {
                expression_model_num_coeffs =
                    eos::cpp17::get<morphablemodel::PcaModel>(morphable_model.get_expression_model().value())
                        .get_num_principal_components();
            }

            int num_expression_coeffs_to_display = std::min(expression_model_num_coeffs, 30);

            if (expression_coefficients.empty() ||
                expression_coefficients.size() != num_expression_coeffs_to_display)
            {
                expression_coefficients.resize(num_expression_coeffs_to_display);
            }
            // expression_coefficients are always initialised now.

            ImGui::BeginGroup();
            for (int i = 0; i < num_expression_coeffs_to_display; ++i)
            {
                const string label = std::to_string(i);
                ImGui::SliderFloat(label.c_str(), &expression_coefficients[i], slider_min_range,
                                   slider_max_range);
            }
            ImGui::EndGroup();
            string coeffs_displayed = "Displaying " + std::to_string(num_expression_coeffs_to_display) + "/" +
                                      std::to_string(expression_model_num_coeffs) + " coefficients.";
            ImGui::Text(coeffs_displayed.c_str());
        }

        ImGui::End(); // end "Expression PCA" window
    };

    viewer.launch();

    return EXIT_SUCCESS;
}
