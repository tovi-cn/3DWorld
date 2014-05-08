// 3D World - Vertex and Fragment GLSL Shader Framework
// by Frank Gennari
// 11/1/10
#include "shaders.h"
#include "mesh.h" // for scene bounds
#include "gl_ext_arb.h"
#include "transform_obj.h"
#include <fstream>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

using namespace std;

bool const PRINT_SHADER = 0; // print shaders loaded from files
bool const DEBUG_SHADER = 0; // print final generated shaders
bool const PRINT_LOG    = 0;

string const shaders_dir = "shaders";
string const shader_prefix_shared_file = "common_header_shared.part*";
string const shader_name_table  [NUM_SHADER_TYPES] = {"vert", "frag", "geom", "tess_control", "tess_eval"};
string const shader_prefix_files[NUM_SHADER_TYPES] = {"common_header", "", "", "", ""}; // always included

shader_t *cur_shader(NULL);

extern bool fog_enabled, mvm_changed, use_core_context;
extern int is_cloudy, display_mode;
extern unsigned enabled_lights;
extern float cur_fog_end;
extern colorRGBA cur_fog_color;
extern gl_light_params_t gl_light_params[MAX_SHADER_LIGHTS];


// *** uniform variables setup ***


char const *append_ix(string &s, unsigned i, bool as_array) {

	assert(i <= 9);
	if (as_array) {s.push_back('[');}
	s.push_back('0'+i);
	if (as_array) {s.push_back(']');}
	return s.c_str();
}


int shader_t::get_uniform_loc(char const *const name) const {

	assert(program && name);
	int const loc(glGetUniformLocation(program, name));
	//cout << "name: " << name << ", loc: " << loc << endl;
	//assert(loc >= 0); // Note: if variable is unused, loc will be -1
	return loc;
}


bool shader_t::set_uniform_float_array(int loc, float const *const val, unsigned num) {
	if (loc >= 0) {glUniform1fv(loc, num, val); return 1;} else {return 0;}
}

bool shader_t::set_uniform_float(int loc, float val) {
	if (loc >= 0) {glUniform1f(loc, val); return 1;} else {return 0;}
}

bool shader_t::set_uniform_int(int loc, int val) {
	if (loc >= 0) {glUniform1i(loc, val); return 1;} else {return 0;}
}

bool shader_t::set_uniform_vector2d(int loc, vector2d const &val) {
	if (loc >= 0) {glUniform2fv(loc, 1, &val.x); return 1;} else {return 0;}
}

bool shader_t::set_uniform_vector3d(int loc, vector3d const &val) { // same as colorRGB
	if (loc >= 0) {glUniform3fv(loc, 1, &val.x); return 1;} else {return 0;}
}

bool shader_t::set_uniform_vector4d(int loc, vector4d const &val) { // same as colorRGBA
	if (loc >= 0) {glUniform4fv(loc, 1, &val.x); return 1;} else {return 0;}
}

bool shader_t::set_uniform_color(int loc, colorRGBA const &val) { // same as vector3d
	if (loc >= 0) {glUniform4fv(loc, 1, &val.R); return 1;} else {return 0;}
}

bool shader_t::set_uniform_color(int loc, colorRGB  const &val) { // same as vector3d
	if (loc >= 0) {glUniform3fv(loc, 1, &val.R); return 1;} else {return 0;}
}

bool shader_t::set_uniform_matrix_3x3(int loc, float const *const m, bool transpose) {
	if (loc >= 0) {glUniformMatrix3fv(loc, 1, transpose, m); return 1;} else {return 0;}
}

bool shader_t::set_uniform_matrix_4x4(int loc, float const *const m, bool transpose) {
	if (loc >= 0) {glUniformMatrix4fv(loc, 1, transpose, m); return 1;} else {return 0;}
}


bool shader_t::add_uniform_float_array(char const *const name, float const *const val, unsigned num) const {
	return set_uniform_float_array(get_uniform_loc(name), val, num);
}

bool shader_t::add_uniform_float(char const *const name, float val) const {
	return set_uniform_float(get_uniform_loc(name), val);
}

bool shader_t::add_uniform_int(char const *const name, int val) const {
	return set_uniform_int(get_uniform_loc(name), val);
}

bool shader_t::add_uniform_vector2d(char const *const name, vector2d const &val) const {
	return set_uniform_vector2d(get_uniform_loc(name), val);
}

bool shader_t::add_uniform_vector3d(char const *const name, vector3d const &val) const {
	return set_uniform_vector3d(get_uniform_loc(name), val);
}

bool shader_t::add_uniform_vector4d(char const *const name, vector4d const &val) const {
	return set_uniform_vector4d(get_uniform_loc(name), val);
}

bool shader_t::add_uniform_color(char const *const name, colorRGBA const &val) const {
	return set_uniform_color(get_uniform_loc(name), val);
}

bool shader_t::add_uniform_color(char const *const name, colorRGB  const &val) const {
	return set_uniform_color(get_uniform_loc(name), val);
}

bool shader_t::add_uniform_matrix_4x4(char const *const name, float *m, bool transpose) const {
	return set_uniform_matrix_4x4(get_uniform_loc(name), m, transpose);
}


// unused, unfinished
bool shader_t::set_uniform_buffer_data(char const *name, float const *data, unsigned size, unsigned &buffer_id) const {

	assert(program && name);
	assert(data && size);

	// There's only one uniform block.
	int const uniformBlockIndex(glGetUniformBlockIndex(program, "uniform_block"));
	if (uniformBlockIndex < 0) return 0;
	//cout << "ix: " << uniformBlockIndex << endl;

	// Associate the uniform block to binding point 0
	glUniformBlockBinding(program, uniformBlockIndex, 0);

	// Get the uniform block's size
	int uniformBlockSize;
	glGetActiveUniformBlockiv(program, uniformBlockIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &uniformBlockSize);
	//cout << "block_size: " << uniformBlockSize << endl;

	// The data uniform will need to be updated, so we'll query its offset/size.
	unsigned index;
	int offset, buf_size;

	// First, get the index for the uniform
	glGetUniformIndices(program, 1, &name, &index);
	assert((int)index >= 0);

	// Use the index to query offset and size
	glGetActiveUniformsiv(program, 1, &index, GL_UNIFORM_OFFSET, &offset);
	glGetActiveUniformsiv(program, 1, &index, GL_UNIFORM_SIZE,   &buf_size);
	//cout << "index: " << index << ", offset: " << offset << ", size: " << buf_size << endl;
	assert(size <= (unsigned)buf_size);
	size = min(size, (unsigned)buf_size);

	// Create UBO
	if (buffer_id == 0 || !glIsBuffer(buffer_id)) glGenBuffers(1, &buffer_id);
	assert(buffer_id > 0);
	glBindBuffer(GL_UNIFORM_BUFFER, buffer_id);
	
	// We can use BufferData to upload our data to the shader, since we know it's in the std140 layout
	glBufferData(GL_UNIFORM_BUFFER, uniformBlockSize, NULL, GL_DYNAMIC_DRAW);

	// Bind constants to UBO binding point 0
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, buffer_id);
	glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
	glBindBuffer(GL_UNIFORM_BUFFER, 0); // unbind
	return 1;
}


// *** attrib variables setup ***


int shader_t::get_attrib_loc(char const *const name, bool allow_fail) const {

	assert(program && name);
	int const loc(glGetAttribLocation(program, name));
	assert(allow_fail || loc >= 0); // Note: if variable is unused, loc will be -1
	return loc;
}


void shader_t::register_attrib_name(char const *const name, unsigned bind_ix) {

	assert(bind_ix < 100); // sanity check
	int const loc(get_attrib_loc(name)); // okay if -1
	if (bind_ix >= attrib_locs.size()) {attrib_locs.resize(bind_ix+1);}
	attrib_locs[bind_ix] = loc;
}


int shader_t::attrib_loc_by_ix(unsigned ix, bool allow_fail) const {

	if (allow_fail && ix >= attrib_locs.size()) return -1; // not set
	assert(ix < attrib_locs.size());
	return attrib_locs[ix]; // is it legal for this to return -1?
}


bool shader_t::set_attrib_float_array(int loc, float const *const val, unsigned num) const {

	if (loc < 0) {return 0;}
	switch (num) {
		case 1: glVertexAttrib1fv(loc, val); break;
		case 2: glVertexAttrib2fv(loc, val); break;
		case 3: glVertexAttrib3fv(loc, val); break;
		case 4: glVertexAttrib4fv(loc, val); break;
		case 16:
			for (unsigned n = 0; n < 4; ++n) {
				glVertexAttrib4fv(loc+n, val+4*n);
			}
			break; // mat4
		default: assert(0);
	}
	return 1;
}

bool shader_t::set_attrib_float(int loc, float val) const {
	if (loc >= 0) {glVertexAttrib1f(loc, val); return 1;} else {return 0;}
}

bool shader_t::set_attrib_int(int loc, int val) const {
	if (loc >= 0) {glVertexAttrib1s(loc, val); return 1;} else {return 0;}
}

bool shader_t::add_attrib_float_array(unsigned ix, float const *const val, unsigned num) const {
	return set_attrib_float_array(attrib_loc_by_ix(ix), val, num);
}

bool shader_t::add_attrib_float(unsigned ix, float val) const {
	return set_attrib_float(attrib_loc_by_ix(ix), val);
}

bool shader_t::add_attrib_int(unsigned ix, int val) const {
	return set_attrib_int(attrib_loc_by_ix(ix), val);
}


// *** other variables setup ***


void shader_t::setup_enabled_lights(unsigned num, unsigned shaders_enabled) {

	assert(num <= MAX_SHADER_LIGHTS);
	prog_name_prefix.push_back(',');
	prog_name_prefix.push_back('L');
	char name[14] = "enable_light0";

	for (unsigned i = 0; i < num; ++i) { // 0=sun, 1=moon, ...
		bool const enabled(is_light_enabled(i));
		prog_name_prefix.push_back(enabled ? '1' : '0');
		name[12] = char('0'+i);
		set_bool_prefixes(name, enabled, shaders_enabled);
	}
}

void shader_t::upload_light_source(unsigned light_id, unsigned field_filt) {

	assert(is_setup());
	assert(light_id < MAX_SHADER_LIGHTS); // only supporting 8 light sources
	if (field_filt == 0 || !is_light_enabled(light_id)) return;
	light_loc_t &lloc(light_locs[light_id]);

	if (!lloc.valid) {
		static char ls_strs[MAX_SHADER_LIGHTS][5][40] = {0};
		static bool is_setup(0);

		if (!is_setup) {
			for (unsigned i = 0; i < MAX_SHADER_LIGHTS; ++i) {
				sprintf(ls_strs[i][0], "fg_LightSource[%u].position", i);
				sprintf(ls_strs[i][1], "fg_LightSource[%u].ambient",  i);
				sprintf(ls_strs[i][2], "fg_LightSource[%u].diffuse",  i);
				sprintf(ls_strs[i][3], "fg_LightSource[%u].specular", i);
				sprintf(ls_strs[i][4], "fg_LightSource[%u].atten",    i);
			}
			is_setup = 1;
		}
		for (unsigned i = 0; i < 5; ++i) {lloc.v[i] = glGetUniformLocation(program, ls_strs[light_id][i]);}
		lloc.valid = 1;
	}
	gl_light_params_t const &lp(gl_light_params[light_id]);
	gl_light_params_t &plp(prev_lps[light_id]);

	if ((field_filt & 0x01) && lloc.v[0] >= 0 && (lp.eye_space_pos != plp.eye_space_pos || lp.pos_w != plp.pos_w)) {
		vector4d const pos(lp.eye_space_pos, lp.pos_w);
		glUniform4fv(lloc.v[0], 1, &pos.x); // in eye space, using MVM at the time set_gl_light_pos() was called
		plp.eye_space_pos = lp.eye_space_pos; plp.pos_w = lp.pos_w;
	}
	if ((field_filt & 0x02) && lloc.v[1] >= 0 && lp.ambient  != plp.ambient ) {glUniform4fv(lloc.v[1], 1, &lp.ambient.R ); plp.ambient  = lp.ambient; }
	if ((field_filt & 0x04) && lloc.v[2] >= 0 && lp.diffuse  != plp.diffuse ) {glUniform4fv(lloc.v[2], 1, &lp.diffuse.R ); plp.diffuse  = lp.diffuse; }
	if ((field_filt & 0x08) && lloc.v[3] >= 0 && lp.specular != plp.specular) {glUniform4fv(lloc.v[3], 1, &lp.specular.R); plp.specular = lp.specular;}
	if ((field_filt & 0x10) && lloc.v[4] >= 0 && lp.atten    != plp.atten   ) {glUniform3fv(lloc.v[4], 1, &lp.atten.x   ); plp.atten    = lp.atten;   }
}

void shader_t::upload_light_sources_range(unsigned start, unsigned end) {

	for (unsigned i = start; i < end; ++i) {
		if (is_light_enabled(i)) {upload_light_source(i);}
	}
}


void shader_t::setup_scene_bounds() const {

	float const scene_zmin(get_zval_min()), scene_zmax(get_zval_max());
	add_uniform_vector3d("scene_llc",   vector3d(-X_SCENE_SIZE, -Y_SCENE_SIZE, scene_zmin));
	add_uniform_vector3d("scene_scale", vector3d(2.0*X_SCENE_SIZE, 2.0*Y_SCENE_SIZE, (scene_zmax - scene_zmin)));
	add_uniform_vector3d("camera_pos",  get_camera_pos());
}

void shader_t::setup_fog_scale() const {

	add_uniform_float("fog_scale", (fog_enabled ? 1.0 : 0.0));
	add_uniform_float("fog_end",   cur_fog_end);
	add_uniform_color("fog_color", cur_fog_color);
}

void shader_t::check_for_fog_disabled() {

	if (!fog_enabled) {for (unsigned d = 0; d < 2; ++d) {set_prefix("#define NO_FOG", d);}} // VS/FS
}


void shader_t::set_prefix(char const *const prefix, unsigned shader_type) {

	assert(shader_type < NUM_SHADER_TYPES);
	prog_name_prefix.push_back(',');
	prog_name_prefix.push_back('s');
	prog_name_prefix.push_back('0'+shader_type);
	prog_name_prefix += prefix;
	prepend_string[shader_type] += prefix;
	prepend_string[shader_type].push_back('\n');
}


void shader_t::set_bool_prefix(char const *const name, bool val, unsigned shader_type) {
	
	set_prefix_str((string("const bool ") + name + (val ? " = true;" : " = false;")), shader_type);
}


void shader_t::set_bool_prefixes(char const *const name, bool val, unsigned shaders_enabled) {

	string const prefix(string("const bool ") + name + (val ? " = true;" : " = false;"));

	for (unsigned s = 0; s < NUM_SHADER_TYPES; ++s) { // put into correct shader(s): V, F, G, TC, TE
		if (shaders_enabled & (1<<s)) {set_prefix_str(prefix, s);}
	}
}


void shader_t::set_int_prefix(char const *const name, int val, unsigned shader_type) {
	
	ostringstream oss;
	oss << val;
	set_prefix_str((string("const int ") + name + " = " + oss.str() + ";"), shader_type);
}


void shader_t::set_color_e(colorRGBA const &color) {
	add_uniform_color("emission", color); // cache the loc to make this faster?
}


void shader_t::set_specular_color(colorRGB const &specular, float shininess) {

	assert(is_setup());
	shininess = max(0.0f, min(128.0f, shininess));
	colorRGBA spec_shine(specular, shininess); // pack specular.rgb and shininess together
	if (is_cloudy && world_mode != WMODE_UNIVERSE) {spec_shine *= 0.5;}

	if (spec_shine != last_spec) {
		add_uniform_color("specular_color", spec_shine);
		last_spec = spec_shine;
	}
}


// *** shader and program setup ***


struct program_t {
	unsigned p, sixs[NUM_SHADER_TYPES];
	bool valid;

	program_t() : p(0), valid(0) {
		for (unsigned i = 0; i < NUM_SHADER_TYPES; ++i) {sixs[i] = 0;}
	}
	program_t(unsigned p_, unsigned sixs_[NUM_SHADER_TYPES]) : p(p_), valid(1) {
		for (unsigned i = 0; i < NUM_SHADER_TYPES; ++i) {sixs[i] = sixs_[i];}
	}
};


class string_prog_map : public map<string, program_t> {
public:
	void clear() {
		free_data();
		map<string, program_t>::clear();
	}
	void free_data() {
		for (const_iterator s = begin(); s != end(); ++s) {
			for (unsigned i = 0; i < NUM_SHADER_TYPES; ++i) {
				if (s->second.sixs[i]) {glDetachShader(s->second.p, s->second.sixs[i]);}
			}
			glDeleteProgram(s->second.p);
		}
	}
	// can't free in the destructor because the gl context may be destroyed before this point
	//~string_prog_map() {free_data();}
};


struct ix_valid_t {
	unsigned ix;
	bool valid;
	ix_valid_t() : ix(0), valid(0) {}
	ix_valid_t(unsigned const ix_) : ix(ix_), valid(1) {}
};


class string_shad_map : public map<string, ix_valid_t> {
public:
	void clear() {
		free_data();
		map<string, ix_valid_t>::clear();
	}
	void free_data() {
		for (const_iterator i = begin(); i != end(); ++i) {
			//assert(glIsShader(i->second.ix));
			glDeleteShader(i->second.ix);
		}
	}
	// can't free in the destructor because the gl context may be destroyed before this point
	//~string_shad_map() {free_data();}
};


class shader_manager_t {

	string_prog_map loaded_programs;
	string_shad_map loaded_shaders[NUM_SHADER_TYPES]; // vertex=0, fragment=1, geometry=2, tess_control=3, tess_eval=4
	map<string, string> loaded_files;

public:
	void clear() {
		loaded_programs.clear();
		for (unsigned d = 0; d < NUM_SHADER_TYPES; ++d) {loaded_shaders[d].clear();}
	}

	void clear_and_reload() {
		clear();
		loaded_files.clear();
	}

	bool load_shader_file(string const &fname, string &data) {
		if (fname.empty()) return 0;
		map<string, string>::const_iterator i(loaded_files.find(fname));
	
		if (i != loaded_files.end()) {
			data += i->second;
			return 1;
		}
		ifstream in(fname.c_str());
		if (!in.good()) return 0;
		string line, file_contents;
		while (std::getline(in, line)) {file_contents += line + '\n';}
		loaded_files[fname] = file_contents;
		if (PRINT_SHADER) cout << "shader data:" << endl << file_contents << endl;
		data += file_contents;
		return 1;
	}

	bool clear_shader_file(string const &fname) {
		assert(!fname.empty());
		map<string, string>::iterator i(loaded_files.find(fname));
		if (i == loaded_files.end()) return 0; // not found - error?
		loaded_files.erase(i);
		return 1;
	}

	static void filename_split(string const &fname, vector<string> &fns, char sep) {
		stringstream ss(fname);
		string fn;
		while (getline(ss, fn, sep)) {fns.push_back(fn);}
	}

	static void get_shader_filenames(string const &name, unsigned type, vector<string> &fns) {
		assert(type < NUM_SHADER_TYPES);
		if (!shader_prefix_shared_file.empty()) {fns.push_back(shader_prefix_shared_file);} // add first, if nonempty
		if (!shader_prefix_files[type].empty()) {fns.push_back(shader_prefix_files[type]);} // add next, if nonempty
		filename_split(name, fns, '+');

		for (vector<string>::iterator i = fns.begin(); i != fns.end(); ++i) {
			assert(!i->empty());
			string fname(shaders_dir + "/" + *i);
		
			if ((*i)[i->size()-1] == '*') { // wildcard shader file: works with all shader types
				fname.erase(fname.size()-1);
			}
			else { // add shader type extension
				fname += "." + shader_name_table[type];
			}
			*i = fname;
		}
	}

	ix_valid_t &get_shader_by_name(string const &name, unsigned type) {return loaded_shaders[type][name];}
	program_t &get_program_by_name(string const &name) {return loaded_programs[name];}
};

shader_manager_t shader_manager;


bool setup_shaders() {

	if (0) {
		int max_pv(0), max_level(0), def_o(0), def_i(0);
		glGetIntegerv(GL_MAX_PATCH_VERTICES, &max_pv);
		glGetIntegerv(GL_MAX_TESS_GEN_LEVEL, &max_level);
		glGetIntegerv(GL_PATCH_DEFAULT_OUTER_LEVEL, &def_o);
		glGetIntegerv(GL_PATCH_DEFAULT_INNER_LEVEL, &def_i);
		cout << "max_pv: " << max_pv << ", max_level: " << max_level << ", def levels: " << def_o << " " << def_i << endl; // 32 64 1 1
		//glPatchParameteri(GL_PATCH_VERTICES, v);
		//glPatchParameteri(GL_PATCH_DEFAULT_OUTER_LEVEL, v);
		//glPatchParameteri(GL_PATCH_DEFAULT_INNER_LEVEL, v);
	}
	cout << "OpenGL Version: " << glGetString(GL_VERSION) << endl;
	cout << "Renderer: " << glGetString(GL_RENDERER) << endl;
	cout << "Vendor: " << glGetString(GL_VENDOR) << endl;
	cout << "GLSL Shader Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << endl;
	if (GLEW_ARB_vertex_shader && GLEW_ARB_fragment_shader && GL_EXT_geometry_shader4) return 1;
	cerr << "Error setting up vertex and fragment GLSL shaders." << endl;
	return 0;
}


void clear_shaders() {

	clear_cached_shaders();
	shader_manager.clear();
}


void reload_all_shaders() { // clears and reloads *everything*

	// Note: do we want/need some function called every frame that check if shader files have been modified and calls this?
	cout << "Reloading all shaders" << endl;
	clear_cached_shaders();
	shader_manager.clear_and_reload();
}


bool yes_no_query(string const &query_str) {

	while (1) {
		cout << query_str << " [Y|N]" << endl;
		string str;
		cin >> str;
		if (str == "Y" || str == "y" || str == "1") {return 1;}
		if (str == "N" || str == "n" || str == "0") {return 0;}
		cout << "Answer not understood" << endl;
	}
	return 0; // never gets here
}


unsigned shader_t::get_shader(string const &name, unsigned type) const {
	
	//RESET_TIME;
	if (name.empty()) return 0; // none selected
	int const shader_type_table[NUM_SHADER_TYPES] = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_GEOMETRY_SHADER, GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER};
	assert(type < NUM_SHADER_TYPES);
	string const lookup_name(name + prepend_string[type]);
	ix_valid_t &ixv(shader_manager.get_shader_by_name(lookup_name, type));
	if (ixv.valid) {return ixv.ix;} // already loaded
	
	// create a new shader
	//string const version_info("#version 400 core\n");
	string const version_info("#version 400\n");
	vector<string> fns;
	shader_manager.get_shader_filenames(name, type, fns);
	bool failed(0);
	unsigned shader(0);

	while (1) { // retry loop
		if (failed) {
			if (!yes_no_query("Retry?")) {
				cerr << "Exiting." << endl;
				exit(1);
			}
			for (vector<string>::const_iterator i = fns.begin(); i != fns.end(); ++i) {
				if (shader_manager.clear_shader_file(*i)) {cout << "Reloading shader component " << *i << endl;}
			}
			failed = 0;
		}
		string data(version_info + prepend_string[type]);

		for (vector<string>::const_iterator i = fns.begin(); i != fns.end(); ++i) {
			if (!shader_manager.load_shader_file(*i, data)) {
				cerr << "Error loading shader file " << *i << "." << endl;
				failed = 1; break;
			}
		}
		if (failed) continue;
		if (DEBUG_SHADER) {cout << "final shader data for <" << name << ">:" << endl << data << endl;}
		shader = glCreateShader(shader_type_table[type]);
	
		if (!shader) {
			cerr << "Error: Failed to create " << shader_name_table[type] << " shader " << name << "." << endl;
			failed = 1; continue;
		}
		const char *src(data.c_str());
		glShaderSource(shader, 1, &src, 0);
		glCompileShader(shader);
		int status(0);
		glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

		if (status != GL_TRUE) {
			cerr << "Compilation of " << shader_name_table[type] << " shader " << name << " failed with status " << status << endl;
			print_shader_info_log(shader);
			cerr << endl;
			failed = 1; continue;
		}
		break;
	} // while(1)
	if (PRINT_LOG) {print_shader_info_log(shader);}
	ixv = ix_valid_t(shader); // cache the shader
	//PRINT_TIME("Create Shader");
	return shader;
}


// See http://www.lighthouse3d.com/tutorials/glsl-core-tutorial/glsl-core-tutorial-create-a-program/
bool shader_t::begin_shader(bool do_enable) {

	//RESET_TIME;
	// get the program
	string input_shaders;
	for (unsigned i = 0; i < NUM_SHADER_TYPES; ++i) {input_shaders += shader_names[i] + ",";} // unique program identifier
	program_t &prog(shader_manager.get_program_by_name(prog_name_prefix + input_shaders));

	if (prog.valid) { // program already exists
		program = prog.p;
	}
	else { // create a new program
		program = glCreateProgram();
		unsigned shader_ixs[NUM_SHADER_TYPES];
		
		for (unsigned i = 0; i < NUM_SHADER_TYPES; ++i) {
			// Note: we *can* attach multiple shaders of the same type to a single program as long as only one has a main()
			shader_ixs[i] = get_shader(shader_names[i], i);
			if (shader_ixs[i]) {glAttachShader(program, shader_ixs[i]);}
		}
		assert(shader_ixs[0] && shader_ixs[1]); // vertex and fragment shaders are required, geometry shader is optional
		if (shader_ixs[2]) {assert(GL_EXT_geometry_shader4);} // geometry shader
		check_gl_error(300);
		glLinkProgram(program);
		int status(0);
		glGetProgramiv(program, GL_LINK_STATUS, &status);

		if (status != GL_TRUE) {
			cerr << "Linking of program " << input_shaders << " failed with status " << status << endl;
			print_program_info_log();
			cout << endl;
			exit(1);
		}
		if (PRINT_LOG) print_program_info_log();
		prog = program_t(program, shader_ixs); // cache the program
		//PRINT_TIME("Create Program");
	}
	cache_vnct_locs();
	cache_matrix_locs();
	if (do_enable) {enable();}
	return 1;
}


void shader_t::print_shader_info_log(unsigned shader) {

	int len(0), len2(0);
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

	if (len > 0) {
		vector<char> info_log_msg(len);
		glGetShaderInfoLog(shader, len, &len2, &info_log_msg.front()); 
		assert(len2 <= len);
		cout << "Info log: " << string(info_log_msg.begin(), info_log_msg.end());
	}
}


void shader_t::print_program_info_log() const {

	int len(0), len2(0);
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);

	if (len > 0) {
		vector<char> info_log_msg(len);
		glGetProgramInfoLog(program, len, &len2, &info_log_msg.front()); 
		assert(len2 <= len);
		cout << "Info log: " << string(info_log_msg.begin(), info_log_msg.end());
	}
}


void shader_t::end_shader() { // ok to call if not in a shader

	disable();
	program = 0;
	
	for (unsigned i = 0; i < NUM_SHADER_TYPES; ++i) {
		prepend_string[i].clear();
		shader_names[i].clear();
	}
	for (unsigned i = 0; i < MAX_SHADER_LIGHTS; ++i) {
		light_locs[i].valid = 0;
		prev_lps[i] = gl_light_params_t(); // reset
	}
	prog_name_prefix.clear();
	attrib_locs.clear();
	last_spec = ALPHA0;
}


void shader_t::enable() {
	
	assert(program);
	glUseProgram(program);
	upload_all_light_sources();
	upload_pjm();
	upload_mvm();
	mvm_changed = 0;
	cur_shader  = this;
}

void shader_t::disable() {
	
	if (is_setup()) {enable_vnct_atribs(0, 0, 0, 0);} // disable all
	glUseProgram(0);
	cur_shader = NULL;
}


// Note 1: We don't handle the case where the projection matrix is updated while a shader is active because this currently doesn't occur.
// Note 2: We assume we only need to update the MVM in at most one active shader, and once updated we can clear the changed flag
void check_mvm_update() {

	if (!mvm_changed) return; // nothing to update
	if (cur_shader) {cur_shader->upload_mvm();}
	mvm_changed = 0;
}

void shader_t::cache_matrix_locs() {

	pm_loc   = get_uniform_loc("fg_ProjectionMatrix"); // okay if returns -1
	mvm_loc  = get_uniform_loc("fg_ModelViewMatrix");
	mvmi_loc = get_uniform_loc("fg_ModelViewMatrixInverse");
	mvpm_loc = get_uniform_loc("fg_ModelViewProjectionMatrix");
	nm_loc   = get_uniform_loc("fg_NormalMatrix");
}

void shader_t::upload_pjm() { // projection matrix

	if (pm_loc >= 0) {set_uniform_matrix_4x4(pm_loc, fgGetPJM().get_ptr(), 0);} // transpose = 0
}

void shader_t::upload_mvm() { // and everything that depends on the mvm

	if (mvm_loc < 0 && mvmi_loc < 0 && mvpm_loc < 0 && nm_loc < 0) return; // nothing to update
	xform_matrix const mvm(fgGetMVM());
	if (mvm_loc >= 0) {set_uniform_matrix_4x4(mvm_loc, mvm.get_ptr(), 0);} // modelview matrix

	if (mvmi_loc >= 0) { // inverse modelview matrix
		xform_matrix mvmi(glm::affineInverse((glm::mat4)mvm));
		set_uniform_matrix_4x4(mvmi_loc, mvmi.get_ptr(), 0);
	}
	if (mvpm_loc >= 0) { // modelview-projection matrix
		xform_matrix const mvp(fgGetPJM() * mvm);
		set_uniform_matrix_4x4(mvpm_loc, mvp.get_ptr(), 0);
	}
	if (nm_loc >= 0) { // normal matrix
		glm::mat3 const nm(glm::inverseTranspose(glm::mat3(mvm)));
		set_uniform_matrix_3x3(nm_loc, glm::value_ptr(nm), 0);
	}
}


// built-in attribute setup/enable/binding
void shader_t::cache_vnct_locs() { // Note: program need not be enabled

	assert(is_setup());
	// Note: locations will generally be {0,1,2,3} as assigned in the vertex shader, but if unused they can be -1
	const char *loc_strs[4] = {"fg_Vertex", "fg_Normal", "fg_Color", "fg_TexCoord"};

	for (unsigned i = 0; i < 4; ++i) {
		vnct_locs[i] = get_attrib_loc(loc_strs[i], 1); // okay if fails
	}
}

void shader_t::enable_vnct_atribs(bool va, bool tca, bool na, bool ca) const { // Note: program must be enabled

	assert(is_setup());
	bool const enables[4] = {va, na, ca, tca};

	for (unsigned i = 0; i < 4; ++i) {
		int const loc(vnct_locs[i]);
		if (loc < 0) continue; // unused in this shader
		if (enables[i]) {glEnableVertexAttribArray(loc);} else {glDisableVertexAttribArray(loc);}
	}
	check_mvm_update();
}

void shader_t::set_vertex_ptr(unsigned stride, void const *const ptr) const {
	assert(vnct_locs[0] >= 0); // vertex must always be available
	glVertexAttribPointer(vnct_locs[0], 3, GL_FLOAT, GL_FALSE, stride, ptr);
}

void shader_t::set_normal_ptr(unsigned stride, void const *const ptr, bool compressed) const {
	if (vnct_locs[1] >= 0) {glVertexAttribPointer(vnct_locs[1], 3, (compressed ? GL_BYTE          : GL_FLOAT), compressed, stride, ptr);}
}

void shader_t::set_color4_ptr(unsigned stride, void const *const ptr, bool compressed) const {
	if (vnct_locs[2] >= 0) {glVertexAttribPointer(vnct_locs[2], 4, (compressed ? GL_UNSIGNED_BYTE : GL_FLOAT), compressed, stride, ptr);}
}

void shader_t::set_tcoord_ptr(unsigned stride, void const *const ptr, bool compressed) const {
	if (vnct_locs[3] >= 0) {glVertexAttribPointer(vnct_locs[3], 2, (compressed ? GL_SHORT         : GL_FLOAT), compressed, stride, ptr);}
}

void shader_t::set_cur_color(colorRGBA const &color) const {
	if (vnct_locs[2] >= 0) {glVertexAttrib4fv(vnct_locs[2], &color.R);}
}


// some simple shared shaders
void shader_t::begin_color_only_shader() {

	set_vert_shader("vert_xform_only");
	set_frag_shader("color_only");
	begin_shader();
}

void shader_t::begin_color_only_shader(colorRGBA const &color) {

	set_vert_shader("vert_xform_only");
	set_frag_shader("color_only");
	begin_shader();
	set_cur_color(color);
}

void shader_t::begin_simple_textured_shader(float min_alpha, bool include_2_lights, bool use_texgen, colorRGBA const *const color) {

	if (include_2_lights) {
		setup_enabled_lights(2, 1); // sun and moon VS lighting
		set_vert_shader(use_texgen ? "ads_lighting.part*+texture_gen.part+two_lights_texture_gen" : "ads_lighting.part*+two_lights_texture");
	}
	else {
		set_vert_shader(use_texgen ? "texture_gen.part+no_lighting_texture_gen" : "no_lighting_tex_coord");
	}
	bool const use_fog(include_2_lights && fog_enabled);
	set_frag_shader(use_fog ? "linear_fog.part+textured_with_fog" : "simple_texture");
	begin_shader();
	if (color) {set_cur_color(*color);}
	add_uniform_float("min_alpha", min_alpha);
	add_uniform_int("tex0", 0);
	if (use_fog) {setup_fog_scale();}
}

void shader_t::begin_untextured_lit_glcolor_shader() {

	begin_simple_textured_shader(0.0, 1); // lighting (not actually textured)
	select_texture(WHITE_TEX); // untextured
}


// **************** INSTANCING + TRANSFORMS ****************


void upload_mvm_to_shader(shader_t &s, char const *const var_name) {

	s.add_uniform_matrix_4x4(var_name, fgGetMVM().get_ptr(), 0);
}


// Note: assumes cur_vbo is currently bound by the caller, and will leave it bound after the call
void instance_render_t::draw_and_clear(int prim_type, unsigned count, unsigned cur_vbo, int index_type, void *indices, unsigned first) { // indices can be NULL

	if (inst_xforms.empty()) return;
	assert(loc >= 0); // Note: could handle this case
	void const *vbo_ptr(get_dynamic_vbo_ptr(inst_xforms.front().get_ptr(), inst_xforms.size()*sizeof(xform_matrix)));
	shader_float_matrix_uploader<4,4>::enable(loc, 1, (float const *)vbo_ptr); // use hardware instancing
	if (cur_vbo) {bind_vbo(cur_vbo);}
	
	if (index_type != GL_NONE) { // indexed
		glDrawElementsInstanced(prim_type, count, index_type, indices, inst_xforms.size());
	}
	else {
		assert(indices == NULL);
		glDrawArraysInstanced(prim_type, first, count, inst_xforms.size());
	}
	shader_float_matrix_uploader<4,4>::disable(loc);
	inst_xforms.clear();
}


void set_point_sprite_mode(bool enabled) { // Note: to be removed when using a core profile

	if (enabled) {
		if (!use_core_context) {glEnable(GL_POINT_SPRITE);}
		glEnable(GL_PROGRAM_POINT_SIZE);
	}
	else {
		if (!use_core_context) {glDisable(GL_POINT_SPRITE);}
		glDisable(GL_PROGRAM_POINT_SIZE);
	}
}

