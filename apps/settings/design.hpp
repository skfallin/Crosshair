#pragma once

// Theme resources keep native focus, contrast themes and control interaction states.
inline constexpr auto designResources = LR"(
<ResourceDictionary xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
                    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
  <ResourceDictionary.ThemeDictionaries>
    <ResourceDictionary x:Key="Dark">
      <SolidColorBrush x:Key="CanvasBrush" Color="#0C0E0F"/>
      <SolidColorBrush x:Key="SidebarBrush" Color="#101213"/>
      <SolidColorBrush x:Key="SurfaceBrush" Color="#191C1E"/>
      <SolidColorBrush x:Key="StrokeBrush" Color="#303537"/>
      <SolidColorBrush x:Key="MutedBrush" Color="#A2ABAD"/>
      <SolidColorBrush x:Key="AquaBrush" Color="#88E9DF"/>
      <SolidColorBrush x:Key="BrandBrush" Color="#C7F928"/>
      <SolidColorBrush x:Key="OnBrandBrush" Color="#13170B"/>
      <SolidColorBrush x:Key="AccentFillColorDefaultBrush" Color="#C7F928"/>
      <SolidColorBrush x:Key="AccentFillColorSecondaryBrush" Color="#B7E723"/>
      <SolidColorBrush x:Key="AccentFillColorTertiaryBrush" Color="#A5D31E"/>
      <SolidColorBrush x:Key="TextOnAccentFillColorPrimaryBrush" Color="#13170B"/>
      <SolidColorBrush x:Key="TextOnAccentFillColorSecondaryBrush" Color="#243010"/>
      <SolidColorBrush x:Key="AccentTextFillColorPrimaryBrush" Color="#C7F928"/>
      <StaticResource x:Key="TextControlBorderBrushFocused" ResourceKey="AccentFillColorDefaultBrush"/>
      <StaticResource x:Key="ContentDialogBackground" ResourceKey="SurfaceBrush"/>
      <StaticResource x:Key="ContentDialogTopOverlay" ResourceKey="SurfaceBrush"/>
      <SolidColorBrush x:Key="ControlStrongStrokeColorDefaultBrush" Color="#8A9495"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundStrokeChecked" ResourceKey="AccentFillColorDefaultBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundStrokeCheckedPointerOver" ResourceKey="AccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundStrokeCheckedPressed" ResourceKey="AccentFillColorTertiaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundStrokeIndeterminate" ResourceKey="AccentFillColorDefaultBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundStrokeIndeterminatePointerOver" ResourceKey="AccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundStrokeIndeterminatePressed" ResourceKey="AccentFillColorTertiaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundFillChecked" ResourceKey="AccentFillColorDefaultBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundFillCheckedPointerOver" ResourceKey="AccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundFillCheckedPressed" ResourceKey="AccentFillColorTertiaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundFillIndeterminate" ResourceKey="AccentFillColorDefaultBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundFillIndeterminatePointerOver" ResourceKey="AccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundFillIndeterminatePressed" ResourceKey="AccentFillColorTertiaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckGlyphForegroundUnchecked" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckGlyphForegroundUncheckedPointerOver" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckGlyphForegroundUncheckedPressed" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckGlyphForegroundChecked" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckGlyphForegroundCheckedPointerOver" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckGlyphForegroundCheckedPressed" ResourceKey="TextOnAccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckGlyphForegroundIndeterminate" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckGlyphForegroundIndeterminatePointerOver" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckGlyphForegroundIndeterminatePressed" ResourceKey="TextOnAccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="ToggleSwitchFillOn" ResourceKey="AccentFillColorDefaultBrush"/>
      <StaticResource x:Key="ToggleSwitchFillOnPointerOver" ResourceKey="AccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="ToggleSwitchFillOnPressed" ResourceKey="AccentFillColorTertiaryBrush"/>
      <StaticResource x:Key="ToggleSwitchStrokeOn" ResourceKey="AccentFillColorDefaultBrush"/>
      <StaticResource x:Key="ToggleSwitchStrokeOnPointerOver" ResourceKey="AccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="ToggleSwitchStrokeOnPressed" ResourceKey="AccentFillColorTertiaryBrush"/>
      <StaticResource x:Key="ToggleSwitchKnobFillOn" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="ToggleSwitchKnobFillOnPointerOver" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="ToggleSwitchKnobFillOnPressed" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="ToggleButtonBackgroundChecked" ResourceKey="AccentFillColorDefaultBrush"/>
      <StaticResource x:Key="ToggleButtonBackgroundCheckedPointerOver" ResourceKey="AccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="ToggleButtonBackgroundCheckedPressed" ResourceKey="AccentFillColorTertiaryBrush"/>
      <StaticResource x:Key="ToggleButtonForegroundChecked" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="ToggleButtonForegroundCheckedPointerOver" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="ToggleButtonForegroundCheckedPressed" ResourceKey="TextOnAccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="AccentButtonBackground" ResourceKey="AccentFillColorDefaultBrush"/>
      <StaticResource x:Key="AccentButtonBackgroundPointerOver" ResourceKey="AccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="AccentButtonBackgroundPressed" ResourceKey="AccentFillColorTertiaryBrush"/>
      <StaticResource x:Key="AccentButtonForeground" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="AccentButtonForegroundPointerOver" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="AccentButtonForegroundPressed" ResourceKey="TextOnAccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="GridViewItemCheckBrush" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="GridViewItemDragForeground" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="GridViewItemSelectedBorderBrush" ResourceKey="AccentFillColorDefaultBrush"/>
      <StaticResource x:Key="GridViewItemSelectedPointerOverBorderBrush" ResourceKey="AccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="GridViewItemSelectedPressedBorderBrush" ResourceKey="AccentFillColorTertiaryBrush"/>
      <StaticResource x:Key="GridViewItemCheckPressedBrush" ResourceKey="TextOnAccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="GridViewItemCheckBoxSelectedBrush" ResourceKey="AccentFillColorDefaultBrush"/>
      <StaticResource x:Key="GridViewItemCheckBoxSelectedPointerOverBrush" ResourceKey="AccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="GridViewItemCheckBoxSelectedPressedBrush" ResourceKey="AccentFillColorTertiaryBrush"/>
    </ResourceDictionary>
)"
                                        LR"(
    <ResourceDictionary x:Key="Light">
      <SolidColorBrush x:Key="CanvasBrush" Color="#F2F4EF"/>
      <SolidColorBrush x:Key="SidebarBrush" Color="#E9EDE5"/>
      <SolidColorBrush x:Key="SurfaceBrush" Color="#FFFFFF"/>
      <SolidColorBrush x:Key="StrokeBrush" Color="#CFD6C9"/>
      <SolidColorBrush x:Key="MutedBrush" Color="#566150"/>
      <SolidColorBrush x:Key="AquaBrush" Color="#146E66"/>
      <SolidColorBrush x:Key="BrandBrush" Color="#C7F928"/>
      <SolidColorBrush x:Key="OnBrandBrush" Color="#13170B"/>
      <SolidColorBrush x:Key="AccentFillColorDefaultBrush" Color="#456100"/>
      <SolidColorBrush x:Key="AccentFillColorSecondaryBrush" Color="#3A5300"/>
      <SolidColorBrush x:Key="AccentFillColorTertiaryBrush" Color="#304600"/>
      <SolidColorBrush x:Key="TextOnAccentFillColorPrimaryBrush" Color="#FFFFFF"/>
      <SolidColorBrush x:Key="TextOnAccentFillColorSecondaryBrush" Color="#FFFFFF"/>
      <SolidColorBrush x:Key="AccentTextFillColorPrimaryBrush" Color="#456100"/>
      <StaticResource x:Key="TextControlBorderBrushFocused" ResourceKey="AccentFillColorDefaultBrush"/>
      <StaticResource x:Key="ContentDialogBackground" ResourceKey="SurfaceBrush"/>
      <StaticResource x:Key="ContentDialogTopOverlay" ResourceKey="SurfaceBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundStrokeChecked" ResourceKey="AccentFillColorDefaultBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundStrokeCheckedPointerOver" ResourceKey="AccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundStrokeCheckedPressed" ResourceKey="AccentFillColorTertiaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundStrokeIndeterminate" ResourceKey="AccentFillColorDefaultBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundStrokeIndeterminatePointerOver" ResourceKey="AccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundStrokeIndeterminatePressed" ResourceKey="AccentFillColorTertiaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundFillChecked" ResourceKey="AccentFillColorDefaultBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundFillCheckedPointerOver" ResourceKey="AccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundFillCheckedPressed" ResourceKey="AccentFillColorTertiaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundFillIndeterminate" ResourceKey="AccentFillColorDefaultBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundFillIndeterminatePointerOver" ResourceKey="AccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckBackgroundFillIndeterminatePressed" ResourceKey="AccentFillColorTertiaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckGlyphForegroundUnchecked" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckGlyphForegroundUncheckedPointerOver" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckGlyphForegroundUncheckedPressed" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckGlyphForegroundChecked" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckGlyphForegroundCheckedPointerOver" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckGlyphForegroundCheckedPressed" ResourceKey="TextOnAccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckGlyphForegroundIndeterminate" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckGlyphForegroundIndeterminatePointerOver" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="CheckBoxCheckGlyphForegroundIndeterminatePressed" ResourceKey="TextOnAccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="ToggleSwitchFillOn" ResourceKey="AccentFillColorDefaultBrush"/>
      <StaticResource x:Key="ToggleSwitchFillOnPointerOver" ResourceKey="AccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="ToggleSwitchFillOnPressed" ResourceKey="AccentFillColorTertiaryBrush"/>
      <StaticResource x:Key="ToggleSwitchStrokeOn" ResourceKey="AccentFillColorDefaultBrush"/>
      <StaticResource x:Key="ToggleSwitchStrokeOnPointerOver" ResourceKey="AccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="ToggleSwitchStrokeOnPressed" ResourceKey="AccentFillColorTertiaryBrush"/>
      <StaticResource x:Key="ToggleSwitchKnobFillOn" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="ToggleSwitchKnobFillOnPointerOver" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="ToggleSwitchKnobFillOnPressed" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="ToggleButtonBackgroundChecked" ResourceKey="AccentFillColorDefaultBrush"/>
      <StaticResource x:Key="ToggleButtonBackgroundCheckedPointerOver" ResourceKey="AccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="ToggleButtonBackgroundCheckedPressed" ResourceKey="AccentFillColorTertiaryBrush"/>
      <StaticResource x:Key="ToggleButtonForegroundChecked" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="ToggleButtonForegroundCheckedPointerOver" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="ToggleButtonForegroundCheckedPressed" ResourceKey="TextOnAccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="AccentButtonBackground" ResourceKey="AccentFillColorDefaultBrush"/>
      <StaticResource x:Key="AccentButtonBackgroundPointerOver" ResourceKey="AccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="AccentButtonBackgroundPressed" ResourceKey="AccentFillColorTertiaryBrush"/>
      <StaticResource x:Key="AccentButtonForeground" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="AccentButtonForegroundPointerOver" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="AccentButtonForegroundPressed" ResourceKey="TextOnAccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="GridViewItemCheckBrush" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="GridViewItemDragForeground" ResourceKey="TextOnAccentFillColorPrimaryBrush"/>
      <StaticResource x:Key="GridViewItemSelectedBorderBrush" ResourceKey="AccentFillColorDefaultBrush"/>
      <StaticResource x:Key="GridViewItemSelectedPointerOverBorderBrush" ResourceKey="AccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="GridViewItemSelectedPressedBorderBrush" ResourceKey="AccentFillColorTertiaryBrush"/>
      <StaticResource x:Key="GridViewItemCheckPressedBrush" ResourceKey="TextOnAccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="GridViewItemCheckBoxSelectedBrush" ResourceKey="AccentFillColorDefaultBrush"/>
      <StaticResource x:Key="GridViewItemCheckBoxSelectedPointerOverBrush" ResourceKey="AccentFillColorSecondaryBrush"/>
      <StaticResource x:Key="GridViewItemCheckBoxSelectedPressedBrush" ResourceKey="AccentFillColorTertiaryBrush"/>
    </ResourceDictionary>
)"
                                        LR"(
    <ResourceDictionary x:Key="HighContrast">
      <SolidColorBrush x:Key="CanvasBrush" Color="{ThemeResource SystemColorWindowColor}"/>
      <SolidColorBrush x:Key="SidebarBrush" Color="{ThemeResource SystemColorWindowColor}"/>
      <SolidColorBrush x:Key="SurfaceBrush" Color="{ThemeResource SystemColorWindowColor}"/>
      <SolidColorBrush x:Key="StrokeBrush" Color="{ThemeResource SystemColorWindowTextColor}"/>
      <SolidColorBrush x:Key="MutedBrush" Color="{ThemeResource SystemColorWindowTextColor}"/>
      <SolidColorBrush x:Key="AquaBrush" Color="{ThemeResource SystemColorWindowTextColor}"/>
      <SolidColorBrush x:Key="BrandBrush" Color="{ThemeResource SystemColorHighlightColor}"/>
      <SolidColorBrush x:Key="OnBrandBrush" Color="{ThemeResource SystemColorHighlightTextColor}"/>
    </ResourceDictionary>
  </ResourceDictionary.ThemeDictionaries>
)"
                                        LR"(
  <Style x:Key="CanvasStyle" TargetType="Grid">
    <Setter Property="Background" Value="{ThemeResource CanvasBrush}"/>
  </Style>
  <Style x:Key="PanelStyle" TargetType="Border">
    <Setter Property="Background" Value="{ThemeResource SurfaceBrush}"/>
    <Setter Property="BorderBrush" Value="{ThemeResource StrokeBrush}"/>
    <Setter Property="BorderThickness" Value="1"/>
    <Setter Property="CornerRadius" Value="12"/>
    <Setter Property="Padding" Value="20"/>
  </Style>
  <Style x:Key="MutedTextStyle" TargetType="TextBlock">
    <Setter Property="Foreground" Value="{ThemeResource MutedBrush}"/>
  </Style>
  <Style x:Key="BrandTextStyle" TargetType="TextBlock">
    <Setter Property="Foreground" Value="{ThemeResource OnBrandBrush}"/>
  </Style>
  <Style x:Key="BrandPanelStyle" TargetType="Border" BasedOn="{StaticResource PanelStyle}">
    <Setter Property="Background" Value="{ThemeResource BrandBrush}"/>
    <Setter Property="BorderBrush" Value="{ThemeResource BrandBrush}"/>
  </Style>
  <Style x:Key="AquaTextStyle" TargetType="TextBlock">
    <Setter Property="Foreground" Value="{ThemeResource AquaBrush}"/>
  </Style>
  <Style TargetType="Button" BasedOn="{StaticResource DefaultButtonStyle}">
    <Setter Property="MinHeight" Value="40"/>
    <Setter Property="CornerRadius" Value="8"/>
    <Setter Property="Padding" Value="14,9"/>
  </Style>
  <Style x:Key="PrimaryButtonStyle" TargetType="Button" BasedOn="{StaticResource AccentButtonStyle}">
    <Setter Property="MinHeight" Value="44"/>
    <Setter Property="CornerRadius" Value="8"/>
    <Setter Property="Padding" Value="24,10"/>
    <Setter Property="FontWeight" Value="SemiBold"/>
  </Style>
  <Style x:Key="NavigationStyle" TargetType="ToggleButton">
    <Setter Property="HorizontalAlignment" Value="Stretch"/>
    <Setter Property="HorizontalContentAlignment" Value="Left"/>
    <Setter Property="MinHeight" Value="48"/>
    <Setter Property="CornerRadius" Value="8"/>
    <Setter Property="Padding" Value="16,12"/>
    <Setter Property="BorderThickness" Value="0"/>
    <Setter Property="Background" Value="Transparent"/>
  </Style>
  <Style TargetType="TextBox">
    <Setter Property="MinHeight" Value="38"/>
    <Setter Property="CornerRadius" Value="6"/>
  </Style>
  <Style TargetType="ComboBox">
    <Setter Property="MinHeight" Value="38"/>
    <Setter Property="HorizontalAlignment" Value="Stretch"/>
    <Setter Property="HorizontalContentAlignment" Value="Stretch"/>
    <Setter Property="CornerRadius" Value="6"/>
  </Style>
  <Style TargetType="Expander">
    <Setter Property="HorizontalAlignment" Value="Stretch"/>
    <Setter Property="HorizontalContentAlignment" Value="Stretch"/>
  </Style>
</ResourceDictionary>)";
