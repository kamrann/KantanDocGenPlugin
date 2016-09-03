KantanDocGen Plugin
-------------

Adds an option to the editor main menu, which brings up a dialog for specifying which modules/packages/classes to document.
Documentation is generated from tooltips/C++ comments.

For functions in blueprints, select a function and fill in the Description box in the details panel (at the top of the 'Graph' category). Unfortunately, at this time UE4 does not support providing descriptions for function parameters in blueprint, so the function description is the only place you can provide customized text for the documentation.

For C++ UFUNCTIONS, use the following comment format above the UFUNCTION() macro. Note that only the function will need to be blueprint exposed in some way to be documented - generally BlueprintCallable, BlueprintPure, BlueprintImplementableEvent or BlueprintNativeEvent.

```
/**
* Function description goes here.
* You can use multiple lines.
*
* @param ParamX Description of parameter ParamX.
* @param ParamY Description of parameter ParamY.
* @return Description of function return value.
*/
UFUNCTION(BlueprintCallable, ...)
int32 SomeFunction(FString ParamX, bool ParamY);
```
This plugin relies on [KantanDocGenTool](https://github.com/kamrann/KantanDocGenTool) for converting intermediate xml form into html. This is packaged inside the plugin so does not need to be installed separately.
