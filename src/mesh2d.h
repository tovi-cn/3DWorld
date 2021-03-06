// 3D World
// by Frank Gennari
// mesh2d class definition
// 3/2/14
#pragma once

#include "3DWorld.h"


struct mesh2d {

	vector<float> pmap; // perturbation map
	vector<unsigned char> rmap; // render map (actually a bool)
	vector<float> emap; // expand map
	vector<point> ptsh; // point shift map
	unsigned size;

	unsigned get_index(unsigned s, unsigned t) const {assert(s < size && t <= size); return (s*(size+1) + t);}

public:
	float expand;

	mesh2d() : size(0), expand(0.0) {}
	void clear();
	unsigned get_num()     const {assert(size > 0); return size*(size+1);} // square, with an extra row
	unsigned choose_rand() const {return (rand() % get_num());}
	void set_size(unsigned sz);
	void alloc_pmap() {pmap.resize(get_num(), 0.0);}
	void add_random(float mag, float min_mag, float max_mag, unsigned skipval=0);
	void mult_by(float val);
	void unset_rand_rmap(unsigned num_remove);
	void set_rand_expand(float mag, unsigned num_exp);
	void set_rand_translate(point const &tp, unsigned num_trans);
	void set_val(unsigned s, unsigned t, float val)      {assert(!pmap.empty()); pmap[get_index(s, t)] = val;}
	float get_val(unsigned s, unsigned t) const          {assert(!pmap.empty()); return pmap[get_index(s, t)];}
	void  set_rm(unsigned s, unsigned t, bool val)       {assert(!rmap.empty()); rmap[get_index(s, t)] = val;}
	bool  get_rm(unsigned s, unsigned t) const           {assert(!rmap.empty()); return (rmap[get_index(s, t)] != 0);}
	void  set_em(unsigned s, unsigned t, float val)      {assert(!emap.empty()); emap[get_index(s, t)] = val;}
	float get_em(unsigned s, unsigned t) const           {assert(!emap.empty()); return emap[get_index(s, t)];}
	void  set_pt(unsigned s, unsigned t, point const &p) {assert(!ptsh.empty()); ptsh[get_index(s, t)] = p;}
	point get_pt(unsigned s, unsigned t) const           {assert(!ptsh.empty()); return ptsh[get_index(s, t)];}
	unsigned get_size() const {return size;}
	void draw_perturbed_sphere(point const &pos, float radius, int ndiv, bool tex_coord) const;
};

