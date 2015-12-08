/* @@trigger: open */

::open:entry
{
    self->data = copyin(arg0, 8);
    self->l = *(uint64_t *)self->data;
    self->w = *(uint32_t *)self->data;
    self->h = *(uint16_t *)self->data;
    self->b = *(uint8_t *)self->data;
    exit(0);
}
