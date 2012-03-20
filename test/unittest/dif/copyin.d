::open:entry
{
    self->data = copyin(arg0, 8);
    exit(0);
}
