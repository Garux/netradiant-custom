#if !defined( INCLUDED_RECT_T_H )
#define INCLUDED_RECT_T_H

struct rect_t {
	float min[2];
	float max[2];

	enum EModifier {
		eSelect,
		eDeselect,
		eToggle,
	};
	EModifier modifier;

	rect_t(){
		min[0] = min[1] = max[0] = max[1] = 0;
		modifier = eSelect;
	}
};


#endif
