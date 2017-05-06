/*
 * eos - A 3D Morphable Model fitting library written in modern C++11/14.
 *
 * File: eos-model-viewer.cpp
 *
 * Copyright 2017 Patrik Huber
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
#include "eos/core/Mesh.hpp"
#include "eos/morphablemodel/MorphableModel.hpp"
#include "eos/morphablemodel/io/cvssp.hpp"
#include "eos/morphablemodel/Blendshape.hpp"

#include <igl/viewer/Viewer.h>
#include "nanogui/slider.h"

#include "boost/program_options.hpp"
#include "boost/filesystem.hpp"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
//#include <algorithm>
#include <map>

using namespace eos;
namespace po = boost::program_options;
namespace fs = boost::filesystem;
using std::cout;
using std::endl;

template <typename T>
std::string to_string(const T a_value, const int n = 6)
{
	std::ostringstream out;
	out << std::setprecision(n) << a_value;
	return out.str();
}

/**
 * Model viewer for 3D Morphable Models.
 * 
 * It's working well but does have a few todo's left, and the code is not very polished.
 *
 * If no model and blendshapes are given via command-line, then a file-open dialog will be presented.
 * If the files are given on the command-line, then no dialog will be presented.
 */
int main(int argc, char *argv[])
{
	fs::path model_file, blendshapes_file;
	try {
		po::options_description desc("Allowed options");
		desc.add_options()
			("help,h",
				"display the help message")
			("model,m", po::value<fs::path>(&model_file),
				"an eos 3D Morphable Model stored as cereal BinaryArchive (.bin)")
			("blendshapes,b", po::value<fs::path>(&blendshapes_file),
				"an eos file with blendshapes (.bin)")
			;
		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
		if (vm.count("help")) {
			cout << "Usage: eos-model-viewer [options]" << endl;
			cout << desc;
			return EXIT_SUCCESS;
		}
		po::notify(vm);
	}
	catch (const po::error& e) {
		cout << "Error while parsing command-line arguments: " << e.what() << endl;
		cout << "Use --help to display a list of options." << endl;
		return EXIT_FAILURE;
	}

	// Should do it from the shape instance actually - never compute the Mesh actually!
	auto get_V = [](const core::Mesh& mesh)
	{
		Eigen::MatrixXd V(mesh.vertices.size(), 3);
		for (int i = 0; i < mesh.vertices.size(); ++i)
		{
			V(i, 0) = mesh.vertices[i].x;
			V(i, 1) = mesh.vertices[i].y;
			V(i, 2) = mesh.vertices[i].z;
		}
		return V;
	};
	auto get_F = [](const core::Mesh& mesh)
	{
		Eigen::MatrixXi F(mesh.tvi.size(), 3);
		for (int i = 0; i < mesh.tvi.size(); ++i)
		{
			F(i, 0) = mesh.tvi[i][0];
			F(i, 1) = mesh.tvi[i][1];
			F(i, 2) = mesh.tvi[i][2];
		}
		return F;
	};
	auto get_C = [](const core::Mesh& mesh)
	{
		Eigen::MatrixXd C(mesh.colors.size(), 3);
		for (int i = 0; i < mesh.colors.size(); ++i)
		{
			C(i, 0) = mesh.colors[i].r;
			C(i, 1) = mesh.colors[i].g;
			C(i, 2) = mesh.colors[i].b;
		}
		return C;
	};

	morphablemodel::MorphableModel morphable_model;
	morphablemodel::Blendshapes blendshapes;

	// These are the coefficients of the currently active mesh instance:
	std::vector<float> shape_coefficients;
	std::vector<float> color_coefficients;
	std::vector<float> blendshape_coefficients;

	igl::viewer::Viewer viewer;

	std::default_random_engine rng;

	std::map<nanogui::Slider*, int> sliders; // If we want to set the sliders to zero separately, we need separate maps here.

	auto add_shape_coefficients_slider = [&sliders, &shape_coefficients, &blendshape_coefficients](igl::viewer::Viewer& viewer, const morphablemodel::MorphableModel& morphable_model, const morphablemodel::Blendshapes& blendshapes, std::vector<float>& coefficients, int coefficient_id, std::string coefficient_name) {
		nanogui::Widget *panel = new nanogui::Widget(viewer.ngui->window());
		panel->setLayout(new nanogui::BoxLayout(nanogui::Orientation::Horizontal, nanogui::Alignment::Middle, 0, 20));
		nanogui::Slider* slider = new nanogui::Slider(panel);
		sliders.emplace(slider, coefficient_id);
		slider->setFixedWidth(80);
		slider->setValue(0.0f);
		slider->setRange({ -3.5f, 3.5f });
		//slider->setHighlightedRange({ -1.0f, 1.0f });

		nanogui::TextBox *textBox = new nanogui::TextBox(panel);
		textBox->setFixedSize(Eigen::Vector2i(40, 20));
		textBox->setValue("0");
		textBox->setFontSize(16);
		textBox->setAlignment(nanogui::TextBox::Alignment::Right);
		slider->setCallback([slider, textBox, &morphable_model, &blendshapes, &viewer, &coefficients, &sliders, &shape_coefficients, &blendshape_coefficients](float value) {
			textBox->setValue(to_string(value, 2)); // while dragging the slider
			auto id = sliders[slider]; // Todo: if it doesn't exist, we should rather throw - this inserts a new item into the map!
			coefficients[id] = value;
			// Just update the shape (vertices):
			Eigen::VectorXf shape;
			if (blendshape_coefficients.size() > 0 && blendshapes.size() > 0)
			{
				shape = morphable_model.get_shape_model().draw_sample(shape_coefficients) + morphablemodel::to_matrix(blendshapes) * Eigen::Map<const Eigen::VectorXf>(blendshape_coefficients.data(), blendshape_coefficients.size());
			}
			else {
				shape = morphable_model.get_shape_model().draw_sample(shape_coefficients);
			}
			auto num_vertices = morphable_model.get_shape_model().get_data_dimension() / 3;
			Eigen::Map<Eigen::MatrixXf> shape_reshaped(shape.data(), 3, num_vertices); // Take 3 at a piece, then transpose below. Works. (But is this really faster than a loop?)
			viewer.data.set_vertices(shape_reshaped.transpose().cast<double>());
		});

		return panel;
	};

	auto add_blendshapes_coefficients_slider = [&sliders, &shape_coefficients, &blendshape_coefficients](igl::viewer::Viewer& viewer, const morphablemodel::MorphableModel& morphable_model, const morphablemodel::Blendshapes& blendshapes, std::vector<float>& coefficients, int coefficient_id, std::string coefficient_name) {
		nanogui::Widget *panel = new nanogui::Widget(viewer.ngui->window());
		panel->setLayout(new nanogui::BoxLayout(nanogui::Orientation::Horizontal, nanogui::Alignment::Middle, 0, 20));
		nanogui::Slider* slider = new nanogui::Slider(panel);
		sliders.emplace(slider, coefficient_id);
		slider->setFixedWidth(80);
		slider->setValue(0.0f);
		slider->setRange({ -0.5f, 2.0f });
		//slider->setHighlightedRange({ 0.0f, 1.0f });

		nanogui::TextBox *textBox = new nanogui::TextBox(panel);
		textBox->setFixedSize(Eigen::Vector2i(40, 20));
		textBox->setValue("0");
		textBox->setFontSize(16);
		textBox->setAlignment(nanogui::TextBox::Alignment::Right);
		slider->setCallback([slider, textBox, &morphable_model, &blendshapes, &viewer, &coefficients, &sliders, &shape_coefficients, &blendshape_coefficients](float value) {
			textBox->setValue(to_string(value, 2)); // while dragging the slider
			auto id = sliders[slider]; // if it doesn't exist, we should rather throw - this inserts a new item into the map!
			coefficients[id] = value;
			// Just update the shape (vertices):
			Eigen::VectorXf shape;
			if (blendshape_coefficients.size() > 0 && blendshapes.size() > 0)
			{
				shape = morphable_model.get_shape_model().draw_sample(shape_coefficients) + morphablemodel::to_matrix(blendshapes) * Eigen::Map<const Eigen::VectorXf>(blendshape_coefficients.data(), blendshape_coefficients.size());
			}
			else { // No blendshapes - doesn't really make sense, we require loading them. But it's fine.
				shape = morphable_model.get_shape_model().draw_sample(shape_coefficients);
			}
			auto num_vertices = morphable_model.get_shape_model().get_data_dimension() / 3;
			Eigen::Map<Eigen::MatrixXf> shape_reshaped(shape.data(), 3, num_vertices); // Take 3 at a piece, then transpose below. Works. (But is this really faster than a loop?)
			viewer.data.set_vertices(shape_reshaped.transpose().cast<double>());
		});

		return panel;
	};

	auto add_color_coefficients_slider = [&sliders, &shape_coefficients, &color_coefficients, &blendshape_coefficients](igl::viewer::Viewer& viewer, const morphablemodel::MorphableModel& morphable_model, const morphablemodel::Blendshapes& blendshapes, std::vector<float>& coefficients, int coefficient_id, std::string coefficient_name) {
		nanogui::Widget *panel = new nanogui::Widget(viewer.ngui->window());
		panel->setLayout(new nanogui::BoxLayout(nanogui::Orientation::Horizontal, nanogui::Alignment::Middle, 0, 20));
		nanogui::Slider* slider = new nanogui::Slider(panel);
		sliders.emplace(slider, coefficient_id);
		slider->setFixedWidth(80);
		slider->setValue(0.0f);
		slider->setRange({ -3.5f, 3.5f });
		//slider->setHighlightedRange({ -1.0f, 1.0f });

		nanogui::TextBox *textBox = new nanogui::TextBox(panel);
		textBox->setFixedSize(Eigen::Vector2i(40, 20));
		textBox->setValue("0");
		textBox->setFontSize(16);
		textBox->setAlignment(nanogui::TextBox::Alignment::Right);
		slider->setCallback([slider, textBox, &morphable_model, &blendshapes, &viewer, &coefficients, &sliders, &shape_coefficients, &color_coefficients, &blendshape_coefficients](float value) {
			textBox->setValue(to_string(value, 2)); // while dragging the slider
			auto id = sliders[slider]; // if it doesn't exist, we should rather throw - this inserts a new item into the map!
			coefficients[id] = value;
			// Set the new colour values:
			Eigen::VectorXf color = morphable_model.get_color_model().draw_sample(color_coefficients);
			auto num_vertices = morphable_model.get_color_model().get_data_dimension() / 3;
			Eigen::Map<Eigen::MatrixXf> color_reshaped(color.data(), 3, num_vertices); // Take 3 at a piece, then transpose below. Works. (But is this really faster than a loop?)
			viewer.data.set_colors(color_reshaped.transpose().cast<double>());
		});

		return panel;
	};

	// Extend viewer menu
	viewer.callback_init = [&](igl::viewer::Viewer& viewer)
	{
		// Todo: We could do the following: If a filename is given via cmdline, then don't open the dialogue!
		if (model_file.empty())
			model_file = nanogui::file_dialog({ { "bin", "eos Morphable Model file" }, { "scm", "scm Morphable Model file" } }, false);
		if (model_file.extension() == ".scm") {
			morphable_model = morphablemodel::load_scm_model(model_file.string()); // try?
		} else {
			morphable_model = morphablemodel::load_model(model_file.string()); // try?
		}
		if (blendshapes_file.empty())
			blendshapes_file = nanogui::file_dialog({ { "bin", "eos blendshapes file" } }, false);
		blendshapes = morphablemodel::load_blendshapes(blendshapes_file.string()); // try?
		// Error on load failure: How to make it pop up?
		//auto dlg = new nanogui::MessageDialog(viewer.ngui->window(), nanogui::MessageDialog::Type::Warning, "Title", "This is a warning message");

		// Initialise all coefficients (all zeros):
		shape_coefficients = std::vector<float>(morphable_model.get_shape_model().get_num_principal_components());
		color_coefficients = std::vector<float>(morphable_model.get_color_model().get_num_principal_components()); // Todo: It can have no colour model!
		blendshape_coefficients = std::vector<float>(blendshapes.size()); // Todo: Should make it work without blendshapes!

		// Start off displaying the mean:
		const auto mesh = morphable_model.get_mean();
		viewer.data.set_mesh(get_V(mesh), get_F(mesh));
		viewer.data.set_colors(get_C(mesh));
		viewer.core.align_camera_center(viewer.data.V, viewer.data.F);

		// General:
		viewer.ngui->addWindow(Eigen::Vector2i(10, 580), "Morphable Model");
		// load/save model & blendshapes
		// save obj
		// Draw random sample
		// Load fitting result... (uesful for maybe seeing where something has gone wrong!)
		// see: https://github.com/wjakob/nanogui/blob/master/src/example1.cpp#L283
		//viewer.ngui->addButton("Open Morphable Model", [&morphable_model]() {
		//	std::string file = nanogui::file_dialog({ {"bin", "eos Morphable Model file"} }, false);
		//	morphable_model = morphablemodel::load_model(file);
		//});
		viewer.ngui->addButton("Random face sample", [&]() {
			const auto sample = morphable_model.draw_sample(rng, 1.0f, 1.0f); // This draws both shape and color model - we can improve the speed by not doing that.
			viewer.data.set_vertices(get_V(sample));
			viewer.data.set_colors(get_C(sample));
			// Set the coefficients and sliders to the drawn alpha value: (ok we don't have them - need to use our own random function)
			// Todo.
		});

		viewer.ngui->addButton("Mean", [&]() {
			const auto mean = morphable_model.get_mean(); // This draws both shape and color model - we can improve the speed by not doing that.
			viewer.data.set_vertices(get_V(mean));
			viewer.data.set_colors(get_C(mean));
			// Set the coefficients and sliders to the mean:
			for (auto&& e : shape_coefficients)
				e = 0.0f;
			for (auto&& e : blendshape_coefficients)
				e = 0.0f;
			for (auto&& e : color_coefficients)
				e = 0.0f;
			for (auto&& s : sliders)
				s.first->setValue(0.0f);
		});

		// The Shape PCA window:
		viewer.ngui->addWindow(Eigen::Vector2i(230, 10), "Shape PCA");
		viewer.ngui->addGroup("Coefficients");
		auto num_shape_coeffs_to_display = std::min(morphable_model.get_shape_model().get_num_principal_components(), 30);
		for (int i=0; i < num_shape_coeffs_to_display; ++i)
		{
			viewer.ngui->addWidget(std::to_string(i), add_shape_coefficients_slider(viewer, morphable_model, blendshapes, shape_coefficients, i, std::to_string(i)));
		}
		if (num_shape_coeffs_to_display < morphable_model.get_shape_model().get_num_principal_components())
		{
			nanogui::Label *label = new nanogui::Label(viewer.ngui->window(), "Displaying 30/" + std::to_string(morphable_model.get_shape_model().get_num_principal_components()) + " coefficients.");
			viewer.ngui->addWidget("", label);
		}

		// The Expression Blendshapes window:
		viewer.ngui->addWindow(Eigen::Vector2i(655, 10), "Expression blendshapes");
		viewer.ngui->addGroup("Coefficients");
		for (int i = 0; i < blendshapes.size(); ++i)
		{
			viewer.ngui->addWidget(std::to_string(i), add_blendshapes_coefficients_slider(viewer, morphable_model, blendshapes, blendshape_coefficients, i, std::to_string(i)));
		}

		// The Colour PCA window:
		viewer.ngui->addWindow(Eigen::Vector2i(440, 10), "Colour PCA");
		viewer.ngui->addGroup("Coefficients");
		auto num_color_coeffs_to_display = std::min(morphable_model.get_color_model().get_num_principal_components(), 30);
		for (int i = 0; i < num_shape_coeffs_to_display; ++i)
		{
			viewer.ngui->addWidget(std::to_string(i), add_color_coefficients_slider(viewer, morphable_model, blendshapes, color_coefficients, i, std::to_string(i)));
		}
		if (num_color_coeffs_to_display < morphable_model.get_shape_model().get_num_principal_components())
		{
			nanogui::Label *label = new nanogui::Label(viewer.ngui->window(), "Displaying 30/" + std::to_string(morphable_model.get_color_model().get_num_principal_components()) + " coefficients.");
			viewer.ngui->addWidget("", label);
		}

		// call to generate menu
		viewer.screen->performLayout();
		return false;
	};

	viewer.launch();

	return EXIT_SUCCESS;
}
