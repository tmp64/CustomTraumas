# Custom Traumas
This utility mod for Prey (2017) allows other mods to add new traumas to the game.

## How to use it
> Warning! This mod is intended for other modders. It does not add any new
> content by itself.

1. In your mod, add a new trauma in `Ark\Player\Traumas.xml`. Make sure to use a
   new ID and a unique name.
2. In your mod, create a new file `Libs\CustomTraumas\YourModName.xml`. Replace
   `YourModName` with your mod's name.
3. Add new traumas like this:
   ```xml
   <?xml version="1.0"?>
   <TraumaList>
       <Trauma ID="10641886185824515142" Name="SpeedBoost" Class="ArkStatusWellFed" />
   </TraumaList>
   ```

   To select the `Class` value, check `Data\Libs\CustomTraumas\Prey.xml` of this
   mod. Those are the classes that Prey uses.
