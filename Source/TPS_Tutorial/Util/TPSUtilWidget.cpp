// 위젯 유틸 함수 소스


#include "TPSUtilWidget.h"


// visibility를 설정한다.
void TPSUtilWidget::SetVisibility( UWidget* Widget, ESlateVisibility Visibility )
{
	if ( !Widget ) return;
	
	Widget->SetVisibility( Visibility );
}

// 텍스트를 설정한다.
void TPSUtilWidget::SetText( UTextBlock* TextBlock, const FString& Text )
{
	if ( !TextBlock ) return;

	TextBlock->SetText( FText::FromString( Text ) );
}
