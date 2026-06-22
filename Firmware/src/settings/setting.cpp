#include "setting.h"

template <typename T>
Setting<T>::Setting(const char *id, T *data, T defaultValue)
	: id(id),
	  data(data),
	  defaultValue(defaultValue){
		
	  }

template <typename T>
Setting<T>::readSetting(){
		
}
